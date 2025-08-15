// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and eyalabraham
// (Dragon 32 emu core) are thanked profusely.
//
// The Micro-DS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================

#ifndef __TAPE_H__
#define __TAPE_H__

#include    <stdint.h>

extern uint32_t tape_pos;
extern uint8_t  tape_motor;
extern uint8_t  tape_speedup;
extern uint8_t  cas_eof;
extern uint8_t  tape_byte;
extern int      bit_index;
extern int      bit_timing_threshold;
extern int      bit_timing_count;
extern uint32_t read_cassette_counter;

extern void    tape_init(void);
extern uint8_t tape_read(void);
extern uint8_t tape_guess_type(void);

#endif  /* __TAPE_H__ */
