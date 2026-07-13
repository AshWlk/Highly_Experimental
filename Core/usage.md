# PSXCore — PSF Playback Usage

PSXCore emulates the PS1/PS2 IOP (R3000 CPU), SPU/SPU2, and a virtual filesystem.
PSF (PlayStation Sound Format) files are PS-X EXE programs that drive the SPU to produce audio.
This document describes how to use the library to play them back.

---

## PSF File Format Overview

A PSF file has three sections:

1. **Compressed PS-X EXE** (zlib deflate) — the PlayStation executable that drives the SPU
2. **Tags** — UTF-8 key/value metadata (`title`, `artist`, `length`, `fade`, `_lib`, etc.)
3. **Reserved area** (PSF2 only) — an IOP module filesystem containing `.irx` modules

The library handles execution and audio synthesis. Decompression, tag parsing, and timing
are the caller's responsibility.

---

## Step-by-Step Playback

### 1. Provide a BIOS Image

A BIOS image must be set **before** calling `psx_init()`. The image size must be a power of two.

```c
#include "bios.h"

bios_set_image(bios_data, bios_size);
```

**PS1 (PSF1):** A real PS1 BIOS ROM, or an HLE BIOS synthesized with `mkhebios_create()`
from a PS2 BIOS image:

```c
#include "mkhebios.h"

int hle_size;
void *hle_bios = mkhebios_create(ps2_bios_data, &hle_size);
bios_set_image(hle_bios, hle_size);
// ...later...
mkhebios_delete(hle_bios);
```

**PS2 (PSF2):** A real PS2 BIOS ROM is required.

The BIOS image must contain environment variables readable by `bios_getenv()`,
including `ps1preboot` and `ps2preboot` (hex addresses used during preboot).

---

### 2. Initialize the Library (once per process)

```c
#include "psx.h"

if (psx_init() != 0) { /* handle error */ }
```

`psx_init()` validates CPU endianness and integer sizes, then initializes all subsystems
(IOP, R3000, SPU, SPU2, VFS, timers). It will intentionally crash via null dereference
if the library was compiled for the wrong byte order.

---

### 3. Allocate and Initialize State

```c
uint8_t version = 1;  // 1 = PS1/PSF1, 2 = PS2/PSF2

void *state = malloc(psx_get_state_size(version));
psx_clear_state(state, version);
```

`psx_clear_state()` runs the BIOS preboot sequence internally, leaving the CPU at
`PC=0x80010000`, `SP=0x801FFFF0`, ready to receive a PS-X EXE.

---

### 4. Register a File Callback (PSF2 only)

PSF2 embeds an IOP module filesystem. The emulated IOP loads `.irx` modules by path.
Provide a callback that reads from your decompressed PSF2 payload:

```c
// Signature: sint32 callback(void *ctx, const char *path, sint32 offset, char *buf, sint32 len)
// Return value: number of bytes read, or negative on error.
psx_set_readfile(state, my_readfile_callback, my_context);
```

Not needed for PSF1.

---

### 5. Upload the PS-X EXE (PSF1)

Decompress the PSF payload with zlib, then upload it. The data **must include** the
2048-byte PS-X EXE header (starting with the `"PS-X EXE"` magic string).

```c
sint32 r = psx_upload_psxexe(state, exe_data, exe_size);
if (r != 0) { /* handle error */ }
```

`psx_upload_psxexe()` reads the load address, size, and entry point from the header,
copies the program into emulated RAM, sets PC/SP, and auto-detects PAL/NTSC from
region strings (`"North America"`, `"Japan"`, `"Europe"`) in the header.

For PSF2, skip this step — modules are loaded via the `readfile` callback.

#### PSF1 `_lib` tag

If the PSF tag `_lib` is present, load the referenced PSF file and upload its EXE
**before** uploading the main file's EXE (the lib provides shared sound data/code).

---

### 6. Set Refresh Rate (optional)

```c
psx_set_refresh(state, 60);  // 60 for NTSC, 50 for PAL
```

Overrides the rate auto-detected from the PS-X EXE header. The refresh rate affects
interrupt timing but not the SPU output sample rate (always 44100 Hz).

---

### 7. Execute and Collect Audio

Call `psx_execute()` in a loop. It runs the emulator for up to `cycles` CPU cycles
**or** `*sound_samples` stereo sample pairs, whichever limit is hit first.

```c
#define SAMPLES_PER_CALL 1024
sint16 sound_buf[SAMPLES_PER_CALL * 2];  // stereo interleaved (L, R, L, R, ...)

while (still_playing) {
    uint32_t samples_out = SAMPLES_PER_CALL;

    sint32 result = psx_execute(
        state,
        768 * SAMPLES_PER_CALL,  // cycles budget (768 cycles/sample = normal speed)
        sound_buf,
        &samples_out,
        0                        // event_mask (0 = no event logging)
    );

    if (result <= -2) { /* unrecoverable error */ break; }
    if (result == -1) { /* halted (PS2 only)  */ break; }

    // samples_out now holds the actual number of stereo pairs generated (may be < requested)
    send_to_audio_output(sound_buf, samples_out);  // 44100 Hz, 16-bit signed stereo
}
```

**Audio output format:** 44100 Hz, stereo, 16-bit signed (`sint16`), interleaved L/R.

---

### 8. Adjust Playback Speed (optional)

The cycles-per-sample ratio controls tempo. 768 is normal; higher is faster, lower is slower.

```c
#include "iop.h"

iop_set_cycles_per_sample(psx_get_iop_state(state), 768);
```

---

### 9. Timing, Fade, and Stop (your responsibility)

The library does not parse PSF tags or enforce track duration. You must:

- Parse the `length` tag (format: `M:SS.mmm`) to determine when to begin fading
- Parse the `fade` tag (duration in milliseconds) and linearly attenuate the PCM output
- Stop calling `psx_execute()` once the fade is complete

---

## Stem Extraction

The SPU can write each voice's rendered audio — and the reverb return — into separate caller-supplied buffers while the normal mix proceeds unchanged. This lets you capture per-voice "stems" (e.g. for analysis or remixing) without modifying playback logic.

### Setup

Register buffers before calling `psx_execute()`. Each buffer must be large enough to hold `samples * 2` `sint16` values (stereo interleaved L/R):

```c
#include "spu.h"

void *spu_state = psx_get_spu_state(state);  // obtain SPU substate

sint16 voice0_buf[SAMPLES_PER_CALL * 2];
sint16 reverb_buf[SAMPLES_PER_CALL * 2];

// Register a stem buffer for voice 0
spu_set_stem_buf(spu_state, 0, voice0_buf);

// Register a buffer for the reverb return signal
spu_set_reverb_buf(spu_state, reverb_buf);
```

After each `psx_execute()` call the buffers are filled and the pointers have been advanced by `samples_out * 2`. Reset them before the next call:

```c
spu_set_stem_buf(spu_state, 0, voice0_buf);
spu_set_reverb_buf(spu_state, reverb_buf);
```

Or clear all at once to stop capturing:

```c
spu_clear_stem_bufs(spu_state);
spu_clear_reverb_buf(spu_state);
```

### Inspecting Voice State

```c
// Read an SPU hardware register (see SPUREG_* constants in spucore.h)
uint32_t val = spu_getreg(spu_state, SPUREG_KON);

// Read the Key-On register directly
uint32_t kon = spu_get_kon(spu_state);

// Get the current sample start address (SSA) for a voice
uint32_t ssa = spu_get_voice_ssa(spu_state, voice);

// Get the SSA as stored in the hardware register
uint32_t ssa_reg = spu_get_voice_ssa_reg(spu_state, voice);
```

### Scanning SPU RAM for Sample Blocks

`spu_scan_samples()` walks SPU RAM up to the reverb work area and returns the start address of each ADPCM sample block (16-byte blocks identified by the loop-end flag in byte 1):

```c
uint32_t addrs[256];
int count = spu_scan_samples(spu_state, addrs, 256);
for (int i = 0; i < count; i++) {
    printf("sample block at SPU RAM offset 0x%05X\n", addrs[i]);
}
```

### Stem Extraction API

| Function | Description |
|---|---|
| `spu_set_stem_buf(state, voice, buf)` | Register a per-voice stem buffer |
| `spu_clear_stem_bufs(state)` | Clear all per-voice stem buffers |
| `spu_set_reverb_buf(state, buf)` | Register a reverb return buffer |
| `spu_clear_reverb_buf(state)` | Clear the reverb return buffer |
| `spu_getreg(state, n)` | Read SPU register `n` from core 0 |
| `spu_getflag(state, n)` | Read SPU flag `n` from core 0 |
| `spu_get_voice_ssa(state, voice)` | Current sample start address for a voice |
| `spu_get_voice_ssa_reg(state, voice)` | SSA as stored in the hardware register |
| `spu_get_kon(state)` | Read the Key-On register |
| `spu_scan_samples(state, out_addrs, max)` | Scan SPU RAM for ADPCM sample blocks |

---

## Compilation Requirements

All `.c` files must be compiled with `-DEMU_COMPILE` and one of:
- `-DEMU_LITTLE_ENDIAN` (x86, ARM LE, etc.)
- `-DEMU_BIG_ENDIAN` (PowerPC, SPARC, etc.)

On platforms with `<stdint.h>`, also define `-DHAVE_STDINT_H`.

---

## Public API Summary

| Purpose | Function | Header |
|---|---|---|
| Set BIOS image | `bios_set_image()` | `bios.h` |
| HLE BIOS creation | `mkhebios_create()`, `mkhebios_delete()` | `mkhebios.h` |
| Library init | `psx_init()` | `psx.h` |
| State size / alloc | `psx_get_state_size()` | `psx.h` |
| State init (+ preboot) | `psx_clear_state()` | `psx.h` |
| Upload PS-X EXE (PSF1) | `psx_upload_psxexe()` | `psx.h` |
| File I/O callback (PSF2) | `psx_set_readfile()` | `psx.h` |
| Set refresh rate | `psx_set_refresh()` | `psx.h` |
| Run emulator + get audio | `psx_execute()` | `psx.h` |
| Get IOP substate | `psx_get_iop_state()` | `psx.h` |
| Adjust tempo | `iop_set_cycles_per_sample()` | `iop.h` |
| Library version string | `psx_getversion()` | `psx.h` |

