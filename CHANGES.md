# Changes

## Overview

This fork adds **stem extraction** to the SPU emulator — the ability to capture the rendered audio output of each voice channel (and the reverb return) into separate caller-supplied buffers in parallel with the normal mix, without altering the existing rendering path.

## `Core/spucore.c` / `Core/spucore.h`

### State

Two new fields added to `SPUCORE_STATE`:

- `sint16 *stem_bufs[24]` — one pointer per voice; when non-NULL, that voice's volume-scaled stereo samples are written here (clipped to 16-bit) alongside the main mix.
- `sint16 *reverb_buf` — when non-NULL, the reverb return signal (before it is summed back into the main output) is written here.

### Render loop changes

- A voice's mono decode is now activated if either the main mix needs it **or** a stem buffer is registered for that voice, so voices that would otherwise be culled (e.g. muted from the main output) are still decoded when a stem is requested.
- Per-voice stereo samples are clipped and written into `stem_bufs[ch]` inside the existing per-sample loop, adding no extra passes over the data.
- The reverb return is captured into `reverb_buf` before being added to the final output.
- `spucore_render()` advances the stem and reverb buffer pointers after each `RENDERMAX`-sized chunk, consistent with how `buf` and `extinput` are advanced.

### IRQ lookahead fix

`spucore_cycles_until_interrupt()` renders speculatively into a state copy to determine IRQ timing. The branch clears all stem/reverb buffer pointers in that copy before the lookahead render, preventing the speculative pass from writing into (and advancing past the bounds of) the caller's pinned output buffers.

### New API (`Core/spucore.h`)

| Function | Description |
|---|---|
| `spucore_set_stem_buf(state, voice, buf)` | Register a stem buffer for a single voice |
| `spucore_clear_stem_bufs(state)` | Clear all 24 per-voice stem buffer pointers |
| `spucore_set_reverb_buf(state, buf)` | Register a buffer for the reverb return signal |
| `spucore_clear_reverb_buf(state)` | Clear the reverb buffer pointer |
| `spucore_get_voice_ssa(state, voice)` | Return the current sample start address of a voice |
| `spucore_scan_samples(ram, ramsize, reverb_start, out_addrs, max_addrs)` | Walk SPU RAM up to `reverb_start`, returning the start address of each ADPCM sample block found (detected via the loop-end flag in byte 1 of each 16-byte block) |

## `Core/spu.c` / `Core/spu.h`

Thin wrappers at the `spu` layer that delegate to the `spucore` functions above. For the PS2 SPU (for version 2 flag in PSF), `spu_clear_stem_bufs` and `spu_clear_reverb_buf` clear both cores. `spu_scan_samples` computes the correct RAM size (512 KB for PS1 SPU, 2 MB for PS2 SPU) before delegating.

### New API (`Core/spu.h`)

| Function | Description |
|---|---|
| `spu_set_stem_buf(state, voice, buf)` | Register a stem buffer for a single voice |
| `spu_clear_stem_bufs(state)` | Clear stem buffers on all cores |
| `spu_set_reverb_buf(state, buf)` | Register a reverb return buffer |
| `spu_clear_reverb_buf(state)` | Clear the reverb buffer on all cores |
| `spu_getreg(state, n)` | Read an SPU register from core 0 |
| `spu_getflag(state, n)` | Read an SPU flag from core 0 |
| `spu_get_voice_ssa(state, voice)` | Current sample start address for a voice |
| `spu_get_voice_ssa_reg(state, voice)` | SSA value as stored in the hardware register |
| `spu_get_kon(state)` | Read the Key-On register |
| `spu_scan_samples(state, out_addrs, max_addrs)` | Scan SPU RAM for ADPCM sample blocks |

## Reverb

### Formula (`Core/spucore.c`)

The reverb step (`reverb_step22`, formerly `reverb_steadystate22`) now follows the nocash psx-spx reverb formula. The previous implementation followed an early reverse-engineered model. Changes:

- **All-pass filters**: the late reverb is now two cascaded all-pass filters (APF1 → APF2), and the wet output is the APF2 output. Previously, `[mLAPF2]` was computed as `vAPF1·comb − (vAPF1 ^ 0x8000)·[mLAPF1−dAPF1] − vAPF2·[mLAPF2−dAPF2]`, and the wet output was the sum of the raw `[mLAPF1]` and `[mLAPF2]` buffer writes.
- **Reflection (IIR) addressing**: each reflection now reads `[m−2]` and writes `[m]`, as on hardware. Previously it read `[m]` and wrote `[m+2]`, so the stored data was one sample ahead of hardware and every tap reading those buffers had a delay one sample too short.
- **Input scaling**: removed the non-hardware 2/3 attenuation of the reverb input.
- **Reverb master enable**: when disabled, buffer writes are skipped but the output is still computed from the buffer contents (previously the whole step was skipped and the raw buffer values were returned).
- The engine now operates on a `struct SPUCORE_REVERB` and a RAM pointer rather than the whole core state, so it can be shared with the standalone reverb unit below.

These changes are not in the formula and are left as they were: the 39-tap downsampling FIR, the Gaussian 22→44.1 kHz upsampling, and saturation of intermediate values to ±32767.

### Standalone reverb unit

A reverb unit is the same reverb engine with a private work area. It follows an SPU core's reverb settings but processes caller-supplied input, e.g. for a per-stem wet signal.

| Function | Description |
|---|---|
| `spucore_reverb_get_state_size(memsize)` / `spu_reverb_get_state_size(version)` | Size of a reverb unit including its work area |
| `spucore_reverb_clear_state(unit, memsize)` / `spu_reverb_clear_state(unit, version)` | Initialise a unit |
| `spucore_reverb_sync(unit, core_state)` / `spu_reverb_sync(unit, spu_state, core)` | Copy reverb registers, ESA/EEA, EVOL and the reverb enable flag from a core. If any work-area address register changed, clears the unit's buffer and restarts it at ESA |
| `spucore_reverb_render(unit, in, out, samples)` / `spu_reverb_render(...)` | Process stereo 44.1 kHz input (`NULL` = silence) and write the EVOL-scaled wet signal |
