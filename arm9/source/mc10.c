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

#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "MicroDS.h"
#include "MicroUtils.h"
#include "CRC32.h"
#include "cpu.h"
#include "mem.h"
#include "tape.h"
#include "vdg.h"
#include "printf.h"

#define NTSC_SCANLINES      262

u32 micro_line              __attribute__((section(".dtcm"))) = 0;
u32 last_file_size          __attribute__((section(".dtcm"))) = 0;

// ------------------------------------------------------------------------
// Reset the emulation. Load the MICROBASIC into the memory map and reset
// the various peripherals for tape and VDG... and start emulation running!
// ------------------------------------------------------------------------
void micro_reset(void)
{
    micro_line = 0;

    // We might need to change the sample rate
    newStreamSampleRate();

    // Initialize all of the peripherals
    mem_init();
    tape_init();
    vdg_init();

    // --------------------------------------------------
    // Load the MICROCOLOR BASIC or MCXBASIC into memory
    // --------------------------------------------------
    if ((myConfig.machine == MACHINE_MCX) && bMCX_found)
    {
        mem_load_rom(0xc000, MCXBASIC, sizeof(MCXBASIC));   // MCX ROM takes up all 16K
    }
    else
    {
        mem_load_rom(0xc000, MC10BASIC, sizeof(MC10BASIC)); // Mirror of 8K BASIC
        mem_load_rom(0xe000, MC10BASIC, sizeof(MC10BASIC)); // ROM normally runs here
    }

    // Reset the CPU and off we go!!
    cpu_init();
    cpu_reset(1);
    cpu_check_reset();
}


// -------------------------------------------------------------------------
// Run the emulation for exactly 1 scanline of Audio and CPU. When we reach
// the VSYNC, we also draw the entire frame. This is not perfect emulation
// as the frame should be drawn scanline-by-scanline... but good enough.
// -------------------------------------------------------------------------
ITCM_CODE u32 micro_run(void)
{
    // --------------------------------------
    // Process 1 scanline worth of DAC Audio
    // --------------------------------------
    processDirectAudio();

    // ----------------------------------------
    // Execute one scanline of CPU (57 cycles)
    // ----------------------------------------
    cpu_run();

    // --------------------------------------------
    // Are we at the end of the frame? VSync time!
    // --------------------------------------------
    if (++micro_line == NTSC_SCANLINES)
    {
        vdg_render();               // Draw the frame
        micro_line = 0;             // Back to the top
        cpu_cycle_deficit = 0;   // Reset cycles per line
        return 1;                   // End of frame
    }

    return 0; // Not end of frame
}

// End of file
