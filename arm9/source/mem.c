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

// ------------------------------------------------------------------------
// RAM is general is laid out as follows:
//
// 0000-001F Internal MC6803 Registers (Ports, Timers, etc)
// 0020-007F Unmapped 
// 0080-00FF Internal MC6803 RAM (128 bytes built into the CPU)
// 0100-3FFF Unmapped - a real MC-10 returns floating address back as data
// 4000-41FF External RAM (Main video RAM area sits in these 512 bytes)
// 4200-4FFF External RAM for BASIC and program use
// 5000-8FFF Expanded RAM - the 16K RAM module maps here
// 9000-BFFF IO Map though generally only BFFF is used (could map RAM)
// C000-DFFF Mirror of the MICROBASIC ROM (unless 16K ROM loaded)
// E000-FFFF MICROBASIC ROM sits here with the vectors in the last area
// ------------------------------------------------------------------------

extern unsigned int debug[];
uint8_t  Memory[MEMORY_SIZE]; // 64K Memory Space for the MC-10

uint32_t io_start             __attribute__((section(".dtcm"))) = 0x9000; // Can be moved up closer to BFFF to allow for more RAM expansion
uint8_t  counter_read_latch   __attribute__((section(".dtcm"))) = 0x00;   // Reading the high-byte of the Timer-Counter latches the low byte

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
    
    // Default machine is 20K (4K internal + 16K RAM expansion)
    // but we allow an expanded 32K version that maps RAM up
    // to the last 256 byte page of memory before the ROM starts.
    io_start = (myConfig.machine ? 0xbf00:0x9000);
    
    counter_read_latch = 0x00;
}


// ------------------------------------------------------------------------
// Port 2 (memory address 0x0003) has 3 special keyboard keys mapped into 
// it... SHIFT, CONTROL and BREAK. Depending on what value was written to
// Memory[2], we could be scanning for one or more of these special keys.
// ------------------------------------------------------------------------
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

// -------------------------------------------------------------------------
// This is the keyboard port that is normally read at 0xbfff and contains
// all pressable keys on the MC-10 except SHIFT, CONTROL and BREAK which
// are handled by the read at port Memory[3]. The value written to Memory[2]
// will indicate which columns are being scanned (a 0-bit represents an 
// active column). Here is the layout of the keymap:
//
//   Column 0   1   2   3   4   5   6   7
//Row-0:    @   A   B   C   D   E   F   G
//Row-1:    H   I   J   K   L   M   N   O
//Row-2:    P   Q   R   S   T   U   V   W
//Row-3:    X   Y   Z              ENT SPC
//Row-4:    0   1   2   3   4   5   6   7
//Row-5:    8   9   :   ;   ,   -   .   /
// -------------------------------------------------------------------------
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

// =========================================================
// MC6803 REGISTERS SUMMARY
// 
// 00 Direction register associated with I/O port 1
// 01 Direction register associated with I/O port 2
// 02 I/O port 1 (data)
// 03 I/O port 2 (data)
// 08 Timer status register
// 09 Counter high byte
// OA Counter low byte
// OB Timer out high byte
// OC Timer out low byte
// 0D Timer in high byte
// 0E Timer in low byte
// 10 Serial interface format and rate control register
// 11 Serial interface status register
// 12 Serial receive register
// 13 Serial transmit register
// 14 RAM control register
// 15-1F Reserved
// =========================================================

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
            cpu.counter = 0xfff8; // Any write to the high-byte sets this as the counter
            break;
            
        case 0x0A:  // Counter Low Byte
            break;  // The counter is read-only except for the specific write to the high byte directly above
            
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
        
        case 0x09:  // Counter high byte
            Memory[0x08] &= ~TCSR_TOF;
            counter_read_latch = (cpu.counter & 0xFF);
            return (cpu.counter >> 8) & 0xFF;
            break;
            
        case 0x0A:  // Counter low byte (latched)
            return counter_read_latch;
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
