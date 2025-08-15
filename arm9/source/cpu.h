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

#ifndef __CPU_H__
#define __CPU_H__

#include    <stdint.h>

/********************************************************************
 *  CPU run state
 */
typedef enum
{
    CPU_EXEC        = 0,    // Normal state instruction execution
    CPU_HALTED      = 1,    // Is halted
    CPU_SYNC        = 2,    // Waiting in SYNC state (for 'sync' and 'cwai')
    CPU_RESET       = 4,    // Held in reset
    CPU_EXCEPTION   = 5,    // Signal an emulation exception (bad op-code)
} cpu_run_state_t;

/* MC6803 CPU state
 */
typedef struct
{
    /* Last command executed
     */
    cpu_run_state_t cpu_state;

    /* Registers reflecting
     * machine state after last command execution
     */
    union
    {
      struct { uint8_t b,a; } ab;   // 8-bit A and B registers (A is high byte of D and B is low byte)
      uint16_t d;                   // 16-bit D register
    } ab;
    uint16_t    x;
    uint16_t    sp;
    uint16_t    pc;
    
    uint32_t    counter;
    uint32_t    compare;

    /* State after last command execution
     */
    uint8_t reset_asserted;
} cpu_state_t;

extern cpu_state_t cpu;

extern int cycles_this_scanline;

/********************************************************************
 *  CPU module API
 */
void cpu_init(void);
void cpu_halt(int state);
void cpu_reset(int state);
void cpu_firq(int state);
void cpu_irq(int state);
void cpu_check_reset(void);
void cpu_run(void);

#endif  /* __CPU_H__ */
