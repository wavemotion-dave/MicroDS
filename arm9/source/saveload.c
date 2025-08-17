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

#include "lzav.h"

#define MICRO_SAVE_VER   0x0001       // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

u8 CompressBuffer[128*1024];

char szLoadFile[MAX_FILENAME_LEN+1];
char tmpStr[34];

uint8_t spare[16] = {0};

void MicroSaveState()
{
  size_t retVal;

  // Return to the original path
  chdir(initial_path);

  // Init filename = romname and SAV in place of ROM
  DIR* dir = opendir("sav");
  if (dir) closedir(dir);    // Directory exists... close it out and move on.
  else mkdir("sav", 0777);   // Otherwise create the directory...
  sprintf(szLoadFile,"sav/%s", initial_file);

  int len = strlen(szLoadFile);
  szLoadFile[len-3] = 's';
  szLoadFile[len-2] = 'a';
  szLoadFile[len-1] = 'v';

  strcpy(tmpStr,"SAVING...");
  DSPrint(2,0,0,tmpStr);

  FILE *handle = fopen(szLoadFile, "wb+");
  if (handle != NULL)
  {
    // Write Version
    u16 save_ver = MICRO_SAVE_VER;
    retVal = fwrite(&save_ver, sizeof(u16), 1, handle);

    // Write Last Directory Path / Tape File
    if (retVal) retVal = fwrite(&last_path, sizeof(last_path), 1, handle);
    if (retVal) retVal = fwrite(&last_file, sizeof(last_file), 1, handle);
    
    // Write Motorola 6803 CPU
    if (retVal) retVal = fwrite(&cpu, sizeof(cpu), 1, handle);

    // Write VDG vars
    if (retVal) retVal = fwrite(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
    if (retVal) retVal = fwrite(&sam_video_mode,        sizeof(sam_video_mode),         1, handle);
    if (retVal) retVal = fwrite(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
    if (retVal) retVal = fwrite(&current_mode,          sizeof(current_mode),           1, handle);
    
    // Some Cassette stuff
    if (retVal) retVal = fwrite(&tape_pos,              sizeof(tape_pos),               1, handle);
    if (retVal) retVal = fwrite(&tape_motor,            sizeof(tape_motor),             1, handle);
    if (retVal) retVal = fwrite(&tape_speedup,          sizeof(tape_speedup),           1, handle);
    if (retVal) retVal = fwrite(&cas_eof,               sizeof(cas_eof),                1, handle);
    if (retVal) retVal = fwrite(&tape_byte,             sizeof(tape_byte),              1, handle);
    if (retVal) retVal = fwrite(&bit_index,             sizeof(bit_index),              1, handle);
    if (retVal) retVal = fwrite(&bit_timing_threshold,  sizeof(bit_timing_threshold),   1, handle);
    if (retVal) retVal = fwrite(&bit_timing_count,      sizeof(bit_timing_count),       1, handle);
    if (retVal) retVal = fwrite(&read_cassette_counter, sizeof(read_cassette_counter),  1, handle);
    
    // And some MicroDS handling memory
    if (retVal) retVal = fwrite(&micro_line,              sizeof(micro_line),               1, handle);
    if (retVal) retVal = fwrite(&micro_special_key,       sizeof(micro_special_key),        1, handle);
    if (retVal) retVal = fwrite(&last_file_size,          sizeof(last_file_size),           1, handle);
    if (retVal) retVal = fwrite(&micro_scanline_counter,  sizeof(micro_scanline_counter),   1, handle);
    if (retVal) retVal = fwrite(&emuFps,                  sizeof(emuFps),                   1, handle);
    if (retVal) retVal = fwrite(&emuActFrames,            sizeof(emuActFrames),             1, handle);
    if (retVal) retVal = fwrite(&timingFrames,            sizeof(timingFrames),             1, handle);
    if (retVal) retVal = fwrite(&counter_read_latch,      sizeof(counter_read_latch),       1, handle);
    
    // And some spare bytes we can eat into as needed without bumping the SAVE version
    if (retVal) retVal = fwrite(spare,                    15,                               1, handle);    
    
    // -----------------------------------------------------------------------
    // Compress the 64K RAM data using 'high' compression ratio... it's
    // still quite fast for such small memory buffers and gets us under 32K
    // -----------------------------------------------------------------------
    int max_len = lzav_compress_bound_hi( 0x9000 );
    int comp_len = lzav_compress_hi( Memory, CompressBuffer, 0x9000, max_len );

    if (retVal) retVal = fwrite(&comp_len,          sizeof(comp_len), 1, handle);
    if (retVal) retVal = fwrite(&CompressBuffer,    comp_len,         1, handle);

    strcpy(tmpStr, (retVal ? "OK ":"ERR"));
    DSPrint(11,0,0,tmpStr);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(2,0,0,"             ");
  }
  else
  {
      strcpy(tmpStr,"Error opening SAV file ...");
      DSPrint(2,0,0,tmpStr);
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void MicroLoadState()
{
  size_t retVal;

  // Return to the original path
  chdir(initial_path);

  // Init filename = romname and SAV in place of ROM
  DIR* dir = opendir("sav");
  if (dir) closedir(dir);    // Directory exists... close it out and move on.
  else mkdir("sav", 0777);   // Otherwise create the directory...
  sprintf(szLoadFile,"sav/%s", initial_file);

  int len = strlen(szLoadFile);
  szLoadFile[len-3] = 's';
  szLoadFile[len-2] = 'a';
  szLoadFile[len-1] = 'v';
  
  memset(spare, 0x00, sizeof(spare));

  FILE *handle = fopen(szLoadFile, "rb");
  if (handle != NULL)
  {
     strcpy(tmpStr,"LOADING...");
     DSPrint(2,0,0,tmpStr);

    // Read Version
    u16 save_ver = 0xBEEF;
    retVal = fread(&save_ver, sizeof(u16), 1, handle);

    if (save_ver == MICRO_SAVE_VER)
    {
        // Restore Last Directory Path / Tape File
        if (retVal) retVal = fread(&last_path, sizeof(last_path), 1, handle);
        if (retVal) retVal = fread(&last_file, sizeof(last_file), 1, handle);
        
        // ---------------------------------------------------------------
        // If we saved a last path/file, we load it back up if possible....
        // ---------------------------------------------------------------
        if (strlen(last_path) > 1)
        {
            chdir(last_path);
            
            if (strlen(last_file) > 1)
            {
                ReadFileCarefully(last_file, TapeBuffer, sizeof(TapeBuffer), 0);
            }
        }

        // Restore Motorola 6803 CPU
        if (retVal) retVal = fread(&cpu, sizeof(cpu), 1, handle);

        // Restore VDG vars
        if (retVal) retVal = fread(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
        if (retVal) retVal = fread(&sam_video_mode,        sizeof(sam_video_mode),         1, handle);
        if (retVal) retVal = fread(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
        if (retVal) retVal = fread(&current_mode,          sizeof(current_mode),           1, handle);
        
        // Restore Cassette stuff
        if (retVal) retVal = fread(&tape_pos,              sizeof(tape_pos),               1, handle);
        if (retVal) retVal = fread(&tape_motor,            sizeof(tape_motor),             1, handle);
        if (retVal) retVal = fread(&tape_speedup,          sizeof(tape_speedup),           1, handle);
        if (retVal) retVal = fread(&cas_eof,               sizeof(cas_eof),                1, handle);
        if (retVal) retVal = fread(&tape_byte,             sizeof(tape_byte),              1, handle);
        if (retVal) retVal = fread(&bit_index,             sizeof(bit_index),              1, handle);
        if (retVal) retVal = fread(&bit_timing_threshold,  sizeof(bit_timing_threshold),   1, handle);
        if (retVal) retVal = fread(&bit_timing_count,      sizeof(bit_timing_count),       1, handle);
        if (retVal) retVal = fread(&read_cassette_counter, sizeof(read_cassette_counter),  1, handle);
        
        // Restore some MicroDS handling memory
        if (retVal) retVal = fread(&micro_line,              sizeof(micro_line),               1, handle);
        if (retVal) retVal = fread(&micro_special_key,       sizeof(micro_special_key),        1, handle);
        if (retVal) retVal = fread(&last_file_size,          sizeof(last_file_size),           1, handle);
        if (retVal) retVal = fread(&micro_scanline_counter,  sizeof(micro_scanline_counter),   1, handle);
        if (retVal) retVal = fread(&emuFps,                  sizeof(emuFps),                   1, handle);
        if (retVal) retVal = fread(&emuActFrames,            sizeof(emuActFrames),             1, handle);
        if (retVal) retVal = fread(&timingFrames,            sizeof(timingFrames),             1, handle);
        if (retVal) retVal = fread(&counter_read_latch,      sizeof(counter_read_latch),       1, handle);
        
        // And some spare bytes we can eat into as needed without bumping the SAVE version
        if (retVal) retVal = fread(spare,                    15,                               1, handle);    

        // Restore Main RAM memory
        int comp_len = 0;
        if (retVal) retVal = fread(&comp_len,          sizeof(comp_len), 1, handle);
        if (retVal) retVal = fread(&CompressBuffer,    comp_len,         1, handle);

        // ------------------------------------------------------------------
        // Decompress the previously compressed RAM and put it back into the
        // right memory location... this is quite fast all things considered.
        // ------------------------------------------------------------------
        (void)lzav_decompress( CompressBuffer, Memory, comp_len, 0x9000 );
        
        strcpy(tmpStr, (retVal ? "OK ":"ERR"));
        DSPrint(11,0,0,tmpStr);

        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(2,0,0,"             ");
      }
  }
  else
  {
      DSPrint(2,0,0,"NO SAVED GAME");
      WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      DSPrint(2,0,0,"             ");
  }

    fclose(handle);
}

// End of file
