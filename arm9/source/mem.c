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
#include    <nds.h>

#include    "mem.h"
#include    "tape.h"
#include    "MicroDS.h"
#include    "MicroUtils.h"

/* -----------------------------------------
   Module globals
----------------------------------------- */
extern unsigned int debug[];
uint8_t  Memory[MEMORY_SIZE]; // 64K Memory Space

uint32_t io_start __attribute__((section(".dtcm"))) = 0x9000;

/*------------------------------------------------
 * mem_init()
 *
 *  Initialize the memory module
 *
 *  param:  Nothing
 *  return: Nothing
 */
void mem_init(void)
{
    for (int addr = 0; addr < MEMORY_SIZE; addr++ )
    {
        Memory[addr] = 0x00;
        
        if (addr < 0x100)                    Memory[addr] = 0x00;
        if (addr >= 0x100 && addr < 0x4000)  Memory[addr] = 0xFF;
        if (addr >= 0x4000 && addr < 0x9000) Memory[addr] = 0x00;
    }
    
    // Default machine is 20K (4K internal + 16K RAM expansion).
    io_start = (myConfig.machine ? 0xbf00:0x9000);
}


uint8_t read_kbd_lo(void)
{
    uint8_t ret = 0x04; // No RS232 input
    
    for (int i=0; i<kbd_keys_pressed; i++)
    {
        uint8_t scan_code = (uint8_t) kbd_keys[i];
        
        if (((Memory[2] & 0x01) == 0))
        {
            if ((scan_code == KBD_CTRL) || ctrl_key) ret |= 0x02;
        }

        if ((Memory[2] & 0x04) == 0)
        {
            if (scan_code == KBD_BREAK) ret |= 0x02;
        }
        
        if (((Memory[2] & 0x80) == 0))
        {
            if ((scan_code == KBD_SHIFT) || shift_key) ret |= 0x02;
        }
    }
    
    return ~ret;
}

uint8_t read_kbd_hi(void)
{
    uint8_t ret = 0x00;
    
    for (int i=0; i<kbd_keys_pressed; i++)
    {
        uint8_t scan_code = (uint8_t) kbd_keys[i];
        
        if ((Memory[2] & 0x01) == 0)
        {
            if (scan_code == KBD_ATSIGN)ret |= 0x01;
            if (scan_code == KBD_H)     ret |= 0x02;
            if (scan_code == KBD_P)     ret |= 0x04;
            if (scan_code == KBD_X)     ret |= 0x08;
            if (scan_code == KBD_0)     ret |= 0x10;
            if (scan_code == KBD_8)     ret |= 0x20;
        }

        if ((Memory[2] & 0x02) == 0)
        {
            if (scan_code == KBD_A)     ret |= 0x01;
            if (scan_code == KBD_I)     ret |= 0x02;
            if (scan_code == KBD_Q)     ret |= 0x04;
            if (scan_code == KBD_Y)     ret |= 0x08;
            if (scan_code == KBD_1)     ret |= 0x10;
            if (scan_code == KBD_9)     ret |= 0x20;
        }

        if ((Memory[2] & 0x04) == 0)
        {
            if (scan_code == KBD_B)     ret |= 0x01;
            if (scan_code == KBD_J)     ret |= 0x02;
            if (scan_code == KBD_R)     ret |= 0x04;
            if (scan_code == KBD_Z)     ret |= 0x08;
            if (scan_code == KBD_2)     ret |= 0x10;
            if (scan_code == KBD_COLON) ret |= 0x20;
        }

        if ((Memory[2] & 0x08) == 0)
        {
            if (scan_code == KBD_C)     ret |= 0x01;
            if (scan_code == KBD_K)     ret |= 0x02;
            if (scan_code == KBD_S)     ret |= 0x04;
            if (scan_code == KBD_3)     ret |= 0x10;
            if (scan_code == KBD_SEMI)  ret |= 0x20;
        }

        if ((Memory[2] & 0x10) == 0)
        {
            if (scan_code == KBD_D)     ret |= 0x01;
            if (scan_code == KBD_L)     ret |= 0x02;
            if (scan_code == KBD_T)     ret |= 0x04;
            if (scan_code == KBD_4)     ret |= 0x10;
            if (scan_code == KBD_COMMA) ret |= 0x20;
        }

        if ((Memory[2] & 0x20) == 0)
        {
            if (scan_code == KBD_E)     ret |= 0x01;
            if (scan_code == KBD_M)     ret |= 0x02;
            if (scan_code == KBD_U)     ret |= 0x04;
            if (scan_code == KBD_5)     ret |= 0x10;
            if (scan_code == KBD_DASH)  ret |= 0x20;
        }

        if ((Memory[2] & 0x40) == 0)
        {
            if (scan_code == KBD_F)     ret |= 0x01;
            if (scan_code == KBD_N)     ret |= 0x02;
            if (scan_code == KBD_V)     ret |= 0x04;
            if (scan_code == KBD_ENTER) ret |= 0x08;
            if (scan_code == KBD_6)     ret |= 0x10;
            if (scan_code == KBD_PERIOD)ret |= 0x20;
        }

        if ((Memory[2] & 0x80) == 0)
        {
            if (scan_code == KBD_G)     ret |= 0x01;
            if (scan_code == KBD_O)     ret |= 0x02;
            if (scan_code == KBD_W)     ret |= 0x04;
            if (scan_code == KBD_SPACE) ret |= 0x08;
            if (scan_code == KBD_7)     ret |= 0x10;
            if (scan_code == KBD_SLASH) ret |= 0x20;
        }
    }
    
    return ~ret;
}


uint16_t counter_latch = 0x0000;
void cpu_reg_write(int address, int data)
{
    if (address > 0x1F) return;
    
    switch (address)
    {
        case 0x08:  // Timer Control (only bits 0-4 writeable)
            Memory[address] &= ~0x1F;
            Memory[address] |= data & 0x1F;
            break;
            
        case 0x09:  // Counter High Byte
            cpu.counter = 0xfff8;
            counter_latch = data << 8;
            Memory[address] = data;
            break;
            
        case 0x0A:  // Counter Low Byte
            cpu.counter = counter_latch | data;
            Memory[address] = data;
            break;
            
        case 0x0B:  // Compare High Byte
            Memory[0x08] &= ~TCSR_OCF;
            cpu.compare = (uint16_t)data << 8;
            Memory[address] = data;
            break;
            
        case 0x0C:  // Compare Low Byte
            Memory[0x08] &= ~TCSR_OCF;
            cpu.compare |= data;
            Memory[address] = data;
            break;
            
        default:
            Memory[address] = data;
            break;
    }
    
}

uint8_t cpu_reg_read(int address)
{
    if (address > 0x1f) return (address & 0xff);
    switch (address)
    {
        case 0x03: // Keyboard Low and Cassette Input
            return read_kbd_lo() & tape_read();
            break;
        
        case 0x07:
            return Memory[address];
            break;
            
        case 0x09:
            Memory[0x08] &= ~TCSR_TOF;
            return (cpu.counter >> 8) & 0xFF;
            break;
            
        case 0x0A:
            return cpu.counter & 0xFF;
            break;
            
        default:
            return Memory[address];
    }    
}

/*------------------------------------------------
 * mem_load_rom()
 *
 *  Load a memory range from a data buffer.
 *
 *  param:  Memory address start, source data buffer and
 *          number of data elements to load
 */
void mem_load_rom(int addr_start, const uint8_t *buffer, int length)
{
    for (int i = 0; i < length; i++)
    {
        Memory[(i+addr_start)] = buffer[i];
    }
}
