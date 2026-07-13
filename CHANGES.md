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
