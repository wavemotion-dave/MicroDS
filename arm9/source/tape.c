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
#include    <ctype.h>

#include    "tape.h"
#include    "MicroDS.h"
#include    "MicroUtils.h"

#define     BIT_THRESHOLD_HI     8
#define     BIT_THRESHOLD_LO     24
#define     TAPE_PLAY_THRESHOLD  20000

uint32_t tape_pos               __attribute__((section(".dtcm"))) = 0;
uint8_t  tape_motor             __attribute__((section(".dtcm"))) = 0;
uint8_t  tape_speedup           __attribute__((section(".dtcm"))) = 0;
uint8_t  cas_eof                __attribute__((section(".dtcm"))) = 0;
uint8_t  tape_byte              __attribute__((section(".dtcm"))) = 0;
int      bit_index              __attribute__((section(".dtcm"))) = 0;
int      bit_timing_threshold   __attribute__((section(".dtcm"))) = 0;
int      bit_timing_count       __attribute__((section(".dtcm"))) = 0;
uint32_t read_cassette_counter  __attribute__((section(".dtcm"))) = 0;


// -----------------------------------------------------------------------------
// A simple routine to look through the bytes of the tape and try to guess
// as to whether this is a BASIC program (which would use CLOAD) or a Machine
// Language program (which would use CLOADM). We look for printable characters
// and assume that most BASIC programs have more printable characters and lots
// of $ and : characters. For such a simple algorithm, it's about 90% accurate
// and the user can override the file type on a per-game configuration basis.
// -----------------------------------------------------------------------------
uint8_t tape_guess_type(void)
{
    int printable_chars = 0;

    for (int i=0; i < last_file_size; i++)
    {
        char c = toupper(TapeBuffer[i]);
        if (c != 0x55)
        {
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == ':') || (c == '$'))
            {
                printable_chars++;
            }
        }
    }

    // If more than a third are printable... assume normal BASIC program
    if (printable_chars > (last_file_size / 3)) return AUTOLOAD_CLOAD;
    else return AUTOLOAD_CLOADM;
}

// -----------------------------------------------------------------
// At this point we have the entire .C10 tape file loaded up into
// the TapeBuffer[] memory and so we just need to index in to grab
// the next byte (or set the EOF if we are at the end of file).
// -----------------------------------------------------------------
inline uint8_t tape_file_read(void)
{
    if (tape_pos > last_file_size)
    {
        cas_eof = 1;
        tape_motor = 0;
        tape_speedup = 0;
        return 0x00;
    }

    // If we are nearing the end... go back to normal speed
    if ((tape_pos + 512) >= last_file_size)
    {
        tape_motor = 1;
        tape_speedup = 0;
    }

    // Return the next tape byte from the file
    return TapeBuffer[tape_pos++];
}

// ----------------------------------------------------
// Called as part of reading Port2
// Bit 4 of Port 2 (Memory[0x03]) is Cassette Input
// ----------------------------------------------------
uint8_t tape_read(void)
{
    uint8_t data = 0x00;

    // --------------------------------------------------------------
    // Fast counter indicates tape motor - basically if the firmware
    // is hammering reading Port2, we are likely in a tape read
    // situation. The MC-10 doesn't have a tape relay control signal
    // so this is the best way to autodetect tape reading.
    // --------------------------------------------------------------
    if (++read_cassette_counter > TAPE_PLAY_THRESHOLD)
    {
        if (tape_motor == 0) tape_motor = 2;
    }

    if (!tape_motor) return 0xFF;

    if ( bit_index == 0 )
    {
        tape_byte = tape_file_read();

        bit_index = 9;
        bit_timing_threshold = 0;
        bit_timing_count = 0;

        /* Force cassette input bit low
         */
        if ( cas_eof )
        {
            return 0xEF;
        }
    }

    if ( bit_timing_count == bit_timing_threshold )
    {
        if ( tape_byte & 0b00000001 )
        {
            bit_timing_threshold = BIT_THRESHOLD_HI;
        }
        else
        {
            bit_timing_threshold = BIT_THRESHOLD_LO;
        }

        bit_timing_count = 0;

        tape_byte = tape_byte >> 1;
        bit_index--;
    }

    if ( bit_timing_count < (bit_timing_threshold / 2) )
    {
        data = 0xEF;
    }
    else
    {
        data = 0xFF;
    }

    bit_timing_count++;

    return data;
}

/*------------------------------------------------
 * tape_init()
 *
 *  Initialize the Cassette handling module
 *
 *  param:  Nothing
 *  return: Nothing
 */
void tape_init(void)
{
    tape_pos   = 0;
    tape_motor = 0;
    cas_eof    = 0;
    bit_index  = 0;
    tape_speedup = 1;
    bit_timing_threshold = 0;
    bit_timing_count = 0;
    read_cassette_counter = 0;
}

// End of file
