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

#ifndef __MEM_H__
#define __MEM_H__

#include  <stdint.h>
#include  "cpu.h"

#define MEMORY_SIZE    65536       // 64K Byte for the full M6803 memory map (RAM + Registers + MICROBASIC)

extern uint8_t  Memory[MEMORY_SIZE];    // 64K RAM 
extern unsigned int debug[];
extern uint8_t counter_read_latch;

// These are for Register at Memory[8]
#define TCSR_OLVL   0x01  // Output Level
#define TCSR_IEDG   0x02  // Input Edge
#define TCSR_ETOI   0x04  // Enable Timer Overflow Interrupt
#define TCSR_EOCI   0x08  // Enable Output Compare Interrupt
#define TCSR_EICI   0x10  // Enable Input Capture Interrupt
#define TCSR_TOF    0x20  // Timer Overflow Flag
#define TCSR_OCF    0x40  // Output Compare Flag
#define TCSR_ICF    0x80  // Input Capture Flag


/********************************************************************
 *  Memory module API
 */

void mem_init(void);

void mem_load_rom(int addr_start, const uint8_t *buffer, int length);
void cpu_reg_write(int address, int data);
uint8_t cpu_reg_read(int address);
uint8_t read_kbd_lo(void);
uint8_t read_kbd_hi(void);

extern uint32_t io_start;

// For when we know it's a PC read and won't be registers...
#define mem_read_pc(addr) (Memory[addr])

/*------------------------------------------------
 * mem_read()
 *
 *  Read memory address
 *
 *  param:  Memory address
 *  return: Memory content at address
 */
inline __attribute__((always_inline)) uint8_t mem_read(int address)
{
    if (address & 0xFF80)
    {
        if (address >= io_start && address <= 0xbfff) return read_kbd_hi();
        else if (address > 0x100 && address < 0x4000) return (address & 0xFF); // Unmapped region returns floating bus address 
        return Memory[address];
    }
    else
    {
        return cpu_reg_read(address);
    }
}

/*------------------------------------------------
 * mem_write()
 *
 *  Write to memory address
 *
 *  param:  Memory address and data to write
 *  return: Nothing
 */

inline __attribute__((always_inline)) void mem_write(int address, int data)
{
    if (address & 0xFF80)
    {
         if (address >= io_start && address <= 0xbfff)
         {
             extern s16 beeper_vol;
             Memory[0xbfff] = (uint8_t) data;
             if (data & 0x80) 
             {
                 beeper_vol = 0x1AFF;
             }
             else
             {
                 beeper_vol = 0;
             }
         }
         // 20K RAM includes the built-in 4K and the 16K Expansion
         else if (address >= 0x4000 && address < io_start)
         {
            Memory[address] = (uint8_t) data;
         }
         else if (address < 0x100) // RAM internal to the MC6803
         {
            Memory[address] = (uint8_t) data;
         }
    }
    else cpu_reg_write(address, data);
}

#endif  /* __MEM_H__ */
