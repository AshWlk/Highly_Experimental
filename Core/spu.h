/////////////////////////////////////////////////////////////////////////////
//
// spu - Top-level SPU emulation, for SPU and SPU2
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __PSX_SPU_H__
#define __PSX_SPU_H__

#include "emuconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

sint32 EMU_CALL spu_init(void);
/* version = 1 for PS1, 2 for PS2 */
uint32 EMU_CALL spu_get_state_size(uint8 version);
void   EMU_CALL spu_clear_state(void *state, uint8 version);

void   EMU_CALL spu_render    (void *state, sint16 *buf, uint32 samples);
void   EMU_CALL spu_render_ext(void *state, sint16 *buf, sint16 *ext, uint32 samples);
uint16 EMU_CALL spu_lh(void *state, uint32 a);
void   EMU_CALL spu_sh(void *state, uint32 a, uint16 d);

void   EMU_CALL spu_dma(void *state, uint32 core, void *mem, uint32 mem_ofs, uint32 mem_mask, uint32 bytes, int iswrite);

uint32 EMU_CALL spu_cycles_until_interrupt(void *state, uint32 samples);

/*
** Enable/disable main or reverb
*/
void EMU_CALL spu_enable_main(void *state, uint8 enable);
void EMU_CALL spu_enable_reverb(void *state, uint8 enable);

/*
** Stem extraction
*/
void   EMU_CALL spu_set_stem_buf      (void *state, uint32 voice, sint16 *buf);
void   EMU_CALL spu_clear_stem_bufs   (void *state);
void   EMU_CALL spu_set_reverb_buf    (void *state, sint16 *buf);
void   EMU_CALL spu_clear_reverb_buf  (void *state);
uint32 EMU_CALL spu_getreg            (void *state, uint32 n);
int    EMU_CALL spu_getflag           (void *state, uint32 n);
uint32 EMU_CALL spu_get_voice_ssa     (void *state, uint32 voice);
uint32 EMU_CALL spu_get_voice_ssa_reg (void *state, uint32 voice);
uint32 EMU_CALL spu_get_kon           (void *state);
int    EMU_CALL spu_scan_samples      (void *state, uint32 *out_addrs, int max_addrs);

#ifdef __cplusplus
}
#endif

#endif
