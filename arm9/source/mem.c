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

uint8_t  Memory[MEMORY_SIZE]; // 64K Memory Space for the MC-10

uint32_t io_start             __attribute__((section(".dtcm"))) = 0x9000; // Can be moved up closer to BFFF to allow for more RAM expansion
uint8_t  counter_read_latch   __attribute__((section(".dtcm"))) = 0x00;   // Reading the high-byte of the Timer-Counter latches the low byte

uint8_t mcx_ram_bank0         __attribute__((section(".dtcm"))) = 0x00;   // For management of MCX RAM banking
uint8_t mcx_ram_bank1         __attribute__((section(".dtcm"))) = 0x00;   // For management of MCX RAM banking
uint8_t mcx_rom_bank          __attribute__((section(".dtcm"))) = 0x00;   // For management of MCX ROM banking

uint8_t Memory_MCX[0x1000]    __attribute__((section(".dtcm")));          // Used to manage the 4K video buffer on an alternate MCX page of memory

/*------------------------------------------------
 * mem_init()
 *
 *  Initialize the memory module. Clear RAM and
 *  setup the memory boundary based on machine type.
 *
 */
void mem_init(void)
{
    for (int addr = 0; addr < MEMORY_SIZE; addr++ )
    {
        Memory[addr] = 0x00;
        Memory_MCX[addr & 0xFFF] = 0x00;

        if (addr < 0x100)                    Memory[addr] = 0x00;
        if (addr >= 0x100 && addr < 0x4000)  Memory[addr] = 0xFF;
        if (addr >= 0x4000 && addr < 0x9000) Memory[addr] = 0x00;
    }

    // ----------------------------------------------------------
    // Default machine is 20K (4K internal + 16K RAM expansion)
    // but we allow an expanded 32K version that maps RAM up
    // to the last 256 byte page of memory before the ROM starts.
    // ----------------------------------------------------------
    io_start = ((myConfig.machine == MACHINE_32K || myConfig.machine == MACHINE_MCX) ? 0xbf00:0x9000);

    mcx_ram_bank0 = 0x00;
    mcx_ram_bank1 = 0x00;
    mcx_rom_bank  = 0x00;

    counter_read_latch = 0x00;
}


// ------------------------------------------------------------------------
// Port 2 (memory address 0x0003) has 3 special keyboard keys mapped into
// it... SHIFT, CONTROL and BREAK. Depending on what value was written to
// Memory[2], we could be scanning for one or more of these special keys.
// ------------------------------------------------------------------------
ITCM_CODE uint8_t read_kbd_lo(void)
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
ITCM_CODE uint8_t read_kbd_hi(void)
{
    uint8_t ret = 0x00;

    // ------------------------------------------------------
    // For each key that was pressed, we need to run through
    // the scan algorithm. The MC-10 program will write to
    // Memory[2] to set the column(s) they are interested in
    // scanning and this routine will read out the row bit.
    // ------------------------------------------------------
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

    // -------------------------------------------------------------
    // The upper two bits aren't used... we return 11 for those
    // which mirrors the handling found in the MC-10 java emulator.
    // -------------------------------------------------------------
    return ~ret;
}

// --------------------------------------------------------------
// This is called whenever a memory address has been accessed
// that is normally unmapped. This generally returns the low
// byte of the address so accessed - essentially a floating bus.
// --------------------------------------------------------------
ITCM_CODE uint8_t unmapped_memory_read(int address)
{
    if (myConfig.machine == MACHINE_MCX) return Memory[address];
    return (address & 0xFF);
}


// ----------------------------------------------------------------------------
// This is called whenever unmapped memory is written to. Peripherals such
// as the MCX might map RAM into these nooks-and-crannies in the MC-10 system.
// ----------------------------------------------------------------------------
ITCM_CODE void unmapped_memory_write(int address, int data)
{
    if (myConfig.machine == MACHINE_MCX)
    {
        // -------------------------------------------------
        // Never allow writes to the ROM area - and we
        // don't yet support banking of RAM in this region.
        // -------------------------------------------------
        if (address < 0xC000) Memory[address] = data;
    }
}

// ----------------------------------------------------------------------------
// An MCX IO write which may bank RAM or ROM. We keep this out of fast memory
// as it takes up a bit of space and we don't call this very often normally.
// Beyond that... we are only supporting a very limited subset of the RAM bank
// handling - just enough to allow the video memory to be handled separate
// from the main MCXBASIC memory - this allows the 'Large' model to run.
// ----------------------------------------------------------------------------
__attribute__((noinline)) void mcx_io_write(int address, int data)
{
    Memory[address] = data; // These ports can be read back

    switch (address & 1)
    {
        case 0x00:  // RAM Banking
            if (mcx_ram_bank0 != (data & 1))
            {
                mcx_ram_bank0 = (data & 1);
                // ----------------------------------------------------------------
                // I have yet to see any real-world MCX program swap bank 0 out...
                // so until I have something to use, we do nothing here.
                // ----------------------------------------------------------------
            }
            if (mcx_ram_bank1 != ((data & 2) >> 1))
            {
                mcx_ram_bank1 = ((data & 2) >> 1);

                // -----------------------------------------------------------------------------------------------------
                // Switch-a-roo!
                // There are so few programs that take advantage of the banked memory that we are willing to do this
                // swap slowly and keep the normal Memory read/write routines fast - it's not worth adding banking
                // pointers to slow down 99% of software when this works fine for the few MCX programs that need the
                // extra memory... So far, only the MCX Large Memory Model (48K) utilizes this banking.
                //
                // For now we are only swapping 4K of this bank - that's the video memory (4K) at main memory 0x4000.
                // That's all that is needed to get the MCX BASIC 'Large' model to run properly. If a program tries
                // to utilize more memory than the 48K in the secondary bank plus the 4K Video in the primary bank,
                // this emulation won't handle it. If and when the time comes for MCX BASIC games to utilize lots of
                // the swappable RAM, we will adjust (but it's unlikely as the latest MCX board is only 32K in RAM.
                // -----------------------------------------------------------------------------------------------------
                uint32_t tmp;
                uint32_t *s32 = (uint32_t *)(Memory+0x4000);
                uint32_t *d32 = (uint32_t *)(Memory_MCX);
                for (int i=0; i<256; i++) // Copy the 4K Video Memory from 'dtcm' fast memory 16 bytes at a time
                {
                    tmp = *s32;  *s32++ = *d32;  *d32++ = tmp;
                    tmp = *s32;  *s32++ = *d32;  *d32++ = tmp;
                    tmp = *s32;  *s32++ = *d32;  *d32++ = tmp;
                    tmp = *s32;  *s32++ = *d32;  *d32++ = tmp;
                }
            }
            break;

        case 0x01:  // ROM Banking
            if (mcx_rom_bank != (data & 3))
            {
                mcx_rom_bank = (data & 3);

                // ---------------------------------------------------------------
                // The only ROM banking we are supporting is switching in of the
                // stock 8K MICROBASIC which is option [0] on the MCX main menu.
                // ---------------------------------------------------------------
                if (mcx_rom_bank == 2)
                {
                    mem_load_rom(0xc000, MC10BASIC, sizeof(MC10BASIC)); // Mirror of 8K BASIC
                    mem_load_rom(0xe000, MC10BASIC, sizeof(MC10BASIC)); // ROM normally runs here
                    mcx_ram_bank0 = 0;
                    mcx_ram_bank1 = 0;
                    cpu_reset(1);
                    cpu_check_reset();
                }
            }
            break;
    }
}

// -------------------------------------------------------------------------------------------
// In theory the MC-10 responds to all unmapped addresses between 0x8000 and 0xBFFF but the
// convention is for programmers to only use 0xBFFF to allow for future IO mapping (such as
// can be found in the MCX handling which utilizes two registers for RAM/ROM banking.
// -------------------------------------------------------------------------------------------
ITCM_CODE void io_write(int address, int data)
{
    // --------------------------------------------------------------------------------------
    // This is a "poor-man" implementation of MCX banking. We aren't supporting the banking
    // beyond allowing the video memory to be managed by the MCX 'Large Model' in another
    // bank. To that end, the MCXBASIC 'Large' model appears to be swapping the middle bank
    // in order to keep the video memory on the main bank 0 RAM and utilizing bank 1 as the
    // place for BASIC and related vars... this allows for an almost 48K BASIC memory free.
    // --------------------------------------------------------------------------------------
    if (myConfig.machine == MACHINE_MCX && (mcx_rom_bank != 2))
    {
        // ---------------------------------------------
        // Is this a write to the I/O space of the MCX.
        // There are two registers in play here:
        // 0xBF00 - RAM banking
        // 0xBF01 - ROM banking
        // ---------------------------------------------
        if ((address & 0x80) == 0)
        {
            mcx_io_write(address, data);
            return;
        }
    }

    // -------------------------------------------------------
    // Otherwise this is the normal MC-10 VDG/Keyboard port...
    // -------------------------------------------------------
    extern s16 beeper_vol;
    Memory[0xbfff] = (uint8_t) data;
    beeper_vol = (data & 0x80) ? 0x1AFF : 0;
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

ITCM_CODE void cpu_reg_write(int address, int data)
{
    if (address > 0x1F) unmapped_memory_write(address, data);

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

// --------------------------------------------------------------------------
// The main things this needs to handle is the keyboard Shift/Control/Break
// reading plus the Cassette input bit and the Timer/Counter read-back.
// --------------------------------------------------------------------------
ITCM_CODE uint8_t cpu_reg_read(int address)
{
    if (address > 0x1f) return unmapped_memory_read(address); // Possibly floating bus

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
 *  Load a ROM buffer into main Memory[]
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
