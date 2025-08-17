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
#include <nds/fifomessages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fat.h>
#include <maxmod9.h>

#include "MicroDS.h"
#include "MicroUtils.h"
#include "mc10_kbd.h"
#include "debug_kbd.h"
#include "cassette.h"
#include "splash.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "soundbank_bin.h"
#include "screenshot.h"
#include "cpu.h"
#include "mem.h"
#include "tape.h"
#include "printf.h"

// -----------------------------------------------------------------
// Most handy for development of the emulator is a set of 16 R/W
// registers and a couple of index vars... we show this when
// global settings is set to show the 'Debugger'. It's amazing
// how handy these general registers are for emulation development.
// -----------------------------------------------------------------
unsigned int debug[0x10]={0};
u32 DX = 0;
u32 DY = 0;

u8 MC10BASIC[0x2000]          = {0};  // We keep the Tandy MC-10 BASIC/BIOS here (8K ROM)
u8 MCXBASIC[0x4000]           = {0};  // We keep the homebrew MCXBASIC here (16K ROM)

u8 TapeBuffer[MAX_FILE_SIZE];     // This is where we keep the raw untouched file as read from the SD card

// ----------------------------------------------------------------------------
// We track the most recent directory and file loaded... both the initial one
// (for the CRC32) and subsequent additional tape loads (Side 2, Side B, etc)
// ----------------------------------------------------------------------------
static char cmd_line_file[256];
char initial_file[MAX_FILENAME_LEN] = "";
char initial_path[MAX_FILENAME_LEN] = "";
char last_path[MAX_FILENAME_LEN]    = "";
char last_file[MAX_FILENAME_LEN]    = "";

// --------------------------------------------------
// A few housekeeping vars to help with emulation...
// --------------------------------------------------
u8 bottom_screen     = 0;

// ---------------------------------------------------------------------------
// Some timing and frame rate comutations to keep the emulation on pace...
// ---------------------------------------------------------------------------
u16 emuFps          __attribute__((section(".dtcm"))) = 0;
u16 emuActFrames    __attribute__((section(".dtcm"))) = 0;
u16 timingFrames    __attribute__((section(".dtcm"))) = 0;

// ----------------------------------------------------------------------------------
// We must have the MC10 MICROBASIC ROM/BIOS in order to run the emulation
// ----------------------------------------------------------------------------------
u8 bBIOS_found      = false;
u8 bMCX_found       = false;

u8 soundEmuPause     __attribute__((section(".dtcm"))) = 1;       // Set to 1 to pause (mute) sound, 0 is sound unmuted (sound channels active)

// -----------------------------------------------------------------------------------------------
// This set of critical vars is what determines the machine type -
// -----------------------------------------------------------------------------------------------
u8 micro_mode        __attribute__((section(".dtcm"))) = 0;       // See defines for the various modes...
u8 kbd_key           __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the ASCII key (e.g. 'A', 'B', '3', etc)
u16 nds_key          __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the NDS keys from keysCurrent() or similar
u8 last_mapped_key   __attribute__((section(".dtcm"))) = 0;       // The last mapped key which has been pressed - used for key click feedback
u8 kbd_keys_pressed  __attribute__((section(".dtcm"))) = 0;       // Each frame we check for keys pressed - since we can map keyboard keys to the NDS, there may be several pressed at once
u8 kbd_keys[12]      __attribute__((section(".dtcm")));           // Up to 12 possible keys pressed at the same time (we have 12 NDS physical buttons though it's unlikely that more than 2 or maybe 3 would be pressed)

u8 bStartSoundEngine = 0;      // Set to true to unmute sound after 1 frame of rendering...
int bg0, bg1, bg0b, bg1b;      // Some vars for NDS background screen handling
u16 vusCptVBL = 0;             // We use this as a basic timer for the Mario sprite... could be removed if another timer can be utilized
u8 touch_debounce = 0;         // A bit of touch-screen debounce
u8 key_debounce = 0;           // A bit of key debounce to ensure the key is held pressed for a minimum amount of time

// The DS/DSi has 12 keys that can be mapped... we only allow mapping the first 10 (START and SELECT have special meaning for the MC-10 emulation)
u16 NDS_keyMap[12] __attribute__((section(".dtcm"))) = {KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_R, KEY_L, KEY_START, KEY_SELECT};

static char tmp[64];    // For various sprintf() calls

// ------------------------------------------------------------
// Utility function to pause the sound...
// ------------------------------------------------------------
void SoundPause(void)
{
    soundEmuPause = 1;
}

// ------------------------------------------------------------
// Utility function to un pause the sound...
// ------------------------------------------------------------
void SoundUnPause(void)
{
    soundEmuPause = 0;
}

// --------------------------------------------------------------------------------------------
// MAXMOD streaming setup and handling...
// We were using the normal ARM7 sound core but it sounded "scratchy" and so with the help
// of FluBBa, we've swiched over to the maxmod sound core which performs much better.
// --------------------------------------------------------------------------------------------
#define SAMPLE_RATE_NTSC    15580       // To roughly match how many samples (262 scanlines x 60 frames = 15720). We purposely undershoot to keep buffer full.
#define buffer_size         (512+16)    // Enough buffer that we don't have to fill it too often. Must be multiple of 16.

mm_ds_system sys   __attribute__((section(".dtcm")));
mm_stream myStream __attribute__((section(".dtcm")));

#define WAVE_DIRECT_BUF_SIZE 2047
u16 mixer_read      __attribute__((section(".dtcm"))) = 0;
u16 mixer_write     __attribute__((section(".dtcm"))) = 0;
s16 mixer[WAVE_DIRECT_BUF_SIZE+1];

// The games normally run at the proper 100% speed, but user can override from 80% to 130%
u16 GAME_SPEED_NTSC[] __attribute__((section(".dtcm"))) = {546, 497, 455, 416, 420, 607 };

u16 catch_up        __attribute__((section(".dtcm"))) = 0;

// -------------------------------------------------------------------------------------------
// maxmod will call this routine when the buffer is half-empty and requests that
// we fill the sound buffer with more samples. They will request 'len' samples and
// we will fill exactly that many. If the sound is paused, we fill with 'mute' samples.
// -------------------------------------------------------------------------------------------
s16 last_sample __attribute__((section(".dtcm"))) = 0;
int breather    __attribute__((section(".dtcm"))) = 0;
extern u16 dac_output;
ITCM_CODE mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        s16 *p = (s16*)dest;
        for (int i=0; i<len; i++)
        {
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
        }
    }
    else
    {
        s16 *p = (s16*)dest;
        for (int i=0; i<len*2; i++)
        {
            if (mixer_read == mixer_write)
            {
                // Just use the last_sample and ask processDirectAudio() to catch-up
                catch_up = 255;
            }
            else
            {
                last_sample = mixer[mixer_read];
                mixer_read = (mixer_read + 1) & WAVE_DIRECT_BUF_SIZE;
            }
            *p++ = last_sample;
        }
        if (breather) {breather -= (len*2); if (breather < 0) breather = 0;}
    }

    return  len;
}

// --------------------------------------------------------------------------------------------
// This is called when we want to sample the audio directly - we grab 4x sound samples.
// --------------------------------------------------------------------------------------------
s16 mixbuf[4]    __attribute__((section(".dtcm")));
s16 beeper_vol   __attribute__((section(".dtcm"))) = 0x0000;
s16 last_dac     __attribute__((section(".dtcm"))) = 0;
ITCM_CODE void processDirectAudio(void)
{
    u8 num_samples = 2;
    
    if (breather) return;
    
    if (catch_up) {catch_up--; num_samples=6;} // Queue nearly empty... catch up

    for (u8 i=0; i<num_samples; i++)
    {
        mixer[mixer_write] = beeper_vol;
        mixer_write++; mixer_write &= WAVE_DIRECT_BUF_SIZE;
        if (((mixer_write+1)&WAVE_DIRECT_BUF_SIZE) == mixer_read) {breather = 1024; break;} // Let the buffer drain a bit...
    }
}

// -----------------------------------------------------------------------------------------------
// The user can override the core emulation speed from 80% to 130% to make games play faster/slow
// than normal. We must adjust the MaxMode sample frequency to match or else we will not have the
// proper number of samples in our sound buffer... this isn't perfect but it's reasonably good!
// -----------------------------------------------------------------------------------------------
static u8 last_game_speed = 99;
static u32 sample_rate_adjust[] = {100, 110, 120, 130, 90, 80};
void newStreamSampleRate(void)
{
    if (last_game_speed != myConfig.gameSpeed)
    {
        last_game_speed = myConfig.gameSpeed;
        mmStreamClose();

        // Adjust the sample rate to match the core emulation speed... user can override from 80% to 130%
        int sample_rate = (SAMPLE_RATE_NTSC * sample_rate_adjust[myConfig.gameSpeed]) / 100;
        
        myStream.sampling_rate  = sample_rate;            // sample_rate to match the scanline emulation
        myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
        myStream.callback       = OurSoundMixer;          // set callback function
        myStream.format         = MM_STREAM_16BIT_STEREO; // format = stereo 16-bit
        myStream.timer          = MM_TIMER0;              // use hardware timer 0
        myStream.manual         = false;                  // use automatic filling
        mmStreamOpen(&myStream);
    }
}

// -------------------------------------------------------------------------------------------
// Setup the maxmod audio stream - we use this to render the 6-bit DAC + 1-bit Beeper audio
// -------------------------------------------------------------------------------------------
void setupStream(void)
{
    //----------------------------------------------------------------
    //  initialize maxmod with our small 3-effect soundbank
    //----------------------------------------------------------------
    mmInitDefaultMem((mm_addr)soundbank_bin);

    mmLoadEffect(SFX_CLICKNOQUIT);
    mmLoadEffect(SFX_KEYCLICK);
    mmLoadEffect(SFX_MUS_INTRO);

    //----------------------------------------------------------------
    //  open stream
    //----------------------------------------------------------------
    myStream.sampling_rate  = SAMPLE_RATE_NTSC;       // sample_rate for the MC-10 emulation
    myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
    myStream.callback       = OurSoundMixer;          // set callback function
    myStream.format         = MM_STREAM_16BIT_STEREO; // format = stereo 16-bit
    myStream.timer          = MM_TIMER0;              // use hardware timer 0
    myStream.manual         = false;                  // use automatic filling
    mmStreamOpen(&myStream);

    //----------------------------------------------------------------
    //  when using 'automatic' filling, your callback will be triggered
    //  every time half of the wave buffer is processed.
    //
    //  so:
    //  25000 (rate)
    //  ----- = ~21 Hz for a full pass, and ~42hz for half pass
    //  1200  (length)
    //----------------------------------------------------------------
    //  with 'manual' filling, you must call mmStreamUpdate
    //  periodically (and often enough to avoid buffer underruns)
    //----------------------------------------------------------------
}

void sound_chip_reset()
{
    memset(mixer, 0x00, sizeof(mixer));
    mixer_read=0;
    mixer_write=0;
    catch_up = 0;
    breather = 0;
}

// -----------------------------------------------------------------------
// We setup the sound chips - disabling all volumes to start.
// -----------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void)
{
    SoundPause();             // Pause any sound output
    sound_chip_reset();       // Reset the sound generator
    setupStream();            // Setup maxmod stream...
    bStartSoundEngine = 5;    // Volume will 'unpause' after 5 frames in the main loop.
}


// Reset the MC-10 main emulator state vars, timers and such...
void ResetMicroComputer(void)
{
    sound_chip_reset();                   // Reset the sound generator
    micro_reset();                        // Reset the MC-10 emulation

    // -----------------------------------------------------------
    // Timer 1 is used to time frame-to-frame of actual emulation
    // -----------------------------------------------------------
    TIMER1_CR = 0;
    TIMER1_DATA=0;
    TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

    // -----------------------------------------------------------
    // Timer 2 is used to time once per second events
    // -----------------------------------------------------------
    TIMER2_CR=0;
    TIMER2_DATA=0;
    TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
    timingFrames  = 0;
    emuFps=0;

    bottom_screen = 0;
}


// Show debug registers...
void ShowDebugger(void)
{
    u8 idx = 1;
    for (u8 i=0; i<4; i++)
    {
        sprintf(tmp, "D%d %-7ld %04lX  D%d %-7ld %04lX", i, (s32)debug[i], (debug[i] < 0xFFFF ? debug[i]:0xFFFF), 4+i, (s32)debug[4+i], (debug[4+i] < 0xFFFF ? debug[4+i]:0xFFFF));
        DSPrint(0, idx++, 0, tmp);
    }
}


// ------------------------------------------------------------
// The status line shows the status of the Emulation System
// on the top line of the bottom DS display.
// ------------------------------------------------------------
void DisplayStatusLine(void)
{
    if (shift_key)
    {
        DSPrint(6, 23, 2, "@");
    }
    else
    {
        DSPrint(6, 23, 6, " ");
    }

    if (ctrl_key)
    {
        DSPrint(10, 23, 6, "@");
    }
    else
    {
        DSPrint(10, 23, 6, " ");
    }
    
    if (tape_motor == 2)
    {
        // Show cassette in green (playing)
        DSPrint(1, 21, 2, "$%&");
        DSPrint(1, 22, 2, "DEF");
    }
    else
    {
        // Show cassette in white (stopped)
        DSPrint(1, 21, 2, "!\"#");
        DSPrint(1, 22, 2, "ABC");
    }
}


// ------------------------------------------------------------------------
// Show the Mini Menu - highlight the selected row. This can be called
// up directly from the Keyboard Graphic - allows the user to quit
// the current game, set high scores, save/load game state, etc.
// ------------------------------------------------------------------------
u8 mini_menu_items = 0;
void MiniMenuShow(bool bClearScreen, u8 sel)
{
    mini_menu_items = 0;
    if (bClearScreen)
    {
      // ---------------------------------------------------
      // Put up a generic background for this mini-menu...
      // ---------------------------------------------------
      BottomScreenOptions();
    }

    DSPrint(8,7,6,                                           " DS MINI MENU  ");
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " RESET  GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " QUIT   GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " SAVE   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " LOAD   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " GAME   OPTIONS");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " DEFINE KEYS   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " EXIT   MENU   ");  mini_menu_items++;
}

// ------------------------------------------------------------------------
// Handle mini-menu interface...
// ------------------------------------------------------------------------
u8 MiniMenu(void)
{
    u8 retVal = MENU_CHOICE_NONE;
    u8 menuSelection = 0;

    SoundPause();
    while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

    MiniMenuShow(true, menuSelection);

    while (true)
    {
        nds_key = keysCurrent();
        if (nds_key)
        {
            if (nds_key & KEY_UP)
            {
                menuSelection = (menuSelection > 0) ? (menuSelection-1):(mini_menu_items-1);
                MiniMenuShow(false, menuSelection);
            }
            if (nds_key & KEY_DOWN)
            {
                menuSelection = (menuSelection+1) % mini_menu_items;
                MiniMenuShow(false, menuSelection);
            }
            if (nds_key & KEY_A)
            {
                if      (menuSelection == 0) retVal = MENU_CHOICE_RESET_GAME;
                else if (menuSelection == 1) retVal = MENU_CHOICE_END_GAME;
                else if (menuSelection == 2) retVal = MENU_CHOICE_SAVE_GAME;
                else if (menuSelection == 3) retVal = MENU_CHOICE_LOAD_GAME;
                else if (menuSelection == 4) retVal = MENU_CHOICE_GAME_OPTION;
                else if (menuSelection == 5) retVal = MENU_CHOICE_DEFINE_KEYS;
                else if (menuSelection == 6) retVal = MENU_CHOICE_NONE;
                else retVal = MENU_CHOICE_NONE;
                break;
            }
            if (nds_key & KEY_B)
            {
                retVal = MENU_CHOICE_NONE;
                break;
            }

            while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
            WAITVBL;WAITVBL;
        }
    }

    while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
    WAITVBL;WAITVBL;

    if (retVal == MENU_CHOICE_NONE)
    {
        BottomScreenKeyboard();  // Could be generic or overlay...
    }

    SoundUnPause();

    return retVal;
}


// -------------------------------------------------------------------------
// Keyboard handler - mapping DS touch screen virtual keys to keyboard keys
// that we can feed into the key processing handler in mem.c when the
// IO port is read at 0xbfff and port 3.
// -------------------------------------------------------------------------

u8 shift_key = 0;
u8 ctrl_key = 0;
u8 last_kbd_key = 0;

u8 handle_keyboard_press(u16 iTx, u16 iTy)  // Tandy MC-10 keyboard mapping
{
    if ((iTy >= 41) && (iTy < 74))   // Row 1 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 29))   kbd_key = KBD_1;
        else if ((iTx >= 29)  && (iTx < 51))   kbd_key = KBD_2;
        else if ((iTx >= 51)  && (iTx < 73))   kbd_key = KBD_3;
        else if ((iTx >= 73)  && (iTx < 95))   kbd_key = KBD_4;
        else if ((iTx >= 95)  && (iTx < 117))  kbd_key = KBD_5;
        else if ((iTx >= 117) && (iTx < 139))  kbd_key = KBD_6;
        else if ((iTx >= 139) && (iTx < 161))  kbd_key = KBD_7;
        else if ((iTx >= 161) && (iTx < 183))  kbd_key = KBD_8;
        else if ((iTx >= 183) && (iTx < 205))  kbd_key = KBD_9;
        else if ((iTx >= 205) && (iTx < 227))  kbd_key = KBD_0;
        else if ((iTx >= 227) && (iTx < 255))  kbd_key = KBD_COLON;
    }
    else if ((iTy >= 74) && (iTy < 104))  // Row 2 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 29))   kbd_key = KBD_Q;
        else if ((iTx >= 29)  && (iTx < 51))   kbd_key = KBD_W;
        else if ((iTx >= 51)  && (iTx < 73))   kbd_key = KBD_E;
        else if ((iTx >= 73)  && (iTx < 95))   kbd_key = KBD_R;
        else if ((iTx >= 95)  && (iTx < 117))  kbd_key = KBD_T;
        else if ((iTx >= 117) && (iTx < 139))  kbd_key = KBD_Y;
        else if ((iTx >= 139) && (iTx < 161))  kbd_key = KBD_U;
        else if ((iTx >= 161) && (iTx < 183))  kbd_key = KBD_I;
        else if ((iTx >= 183) && (iTx < 205))  kbd_key = KBD_O;
        else if ((iTx >= 205) && (iTx < 227))  kbd_key = KBD_P;
        else if ((iTx >= 227) && (iTx < 255))  kbd_key = KBD_ATSIGN;
    }
    else if ((iTy >= 104) && (iTy < 135)) // Row 3 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 29))   kbd_key = KBD_A;
        else if ((iTx >= 29)  && (iTx < 51))   kbd_key = KBD_S;
        else if ((iTx >= 51)  && (iTx < 73))   kbd_key = KBD_D;
        else if ((iTx >= 73)  && (iTx < 95))   kbd_key = KBD_F;
        else if ((iTx >= 95)  && (iTx < 117))  kbd_key = KBD_G;
        else if ((iTx >= 117) && (iTx < 139))  kbd_key = KBD_H;
        else if ((iTx >= 139) && (iTx < 161))  kbd_key = KBD_J;
        else if ((iTx >= 161) && (iTx < 183))  kbd_key = KBD_K;
        else if ((iTx >= 183) && (iTx < 205))  kbd_key = KBD_L;
        else if ((iTx >= 205) && (iTx < 227))  kbd_key = KBD_SEMI;
        else if ((iTx >= 227) && (iTx < 255))  kbd_key = KBD_DASH;
    }
    else if ((iTy >= 135) && (iTy < 166)) // Row 4 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 29))   kbd_key = KBD_Z;
        else if ((iTx >= 29)  && (iTx < 51))   kbd_key = KBD_X;
        else if ((iTx >= 51)  && (iTx < 73))   kbd_key = KBD_C;
        else if ((iTx >= 73)  && (iTx < 95))   kbd_key = KBD_V;
        else if ((iTx >= 95)  && (iTx < 117))  kbd_key = KBD_B;
        else if ((iTx >= 117) && (iTx < 139))  kbd_key = KBD_N;
        else if ((iTx >= 139) && (iTx < 161))  kbd_key = KBD_M;
        else if ((iTx >= 161) && (iTx < 183))  kbd_key = KBD_COMMA;
        else if ((iTx >= 183) && (iTx < 205))  kbd_key = KBD_PERIOD;
        else if ((iTx >= 205) && (iTx < 227))  kbd_key = KBD_SLASH;
        else if ((iTx >= 227) && (iTx < 255))  kbd_key = KBD_BREAK;
    }
    else if ((iTy >= 166) && (iTy < 192)) // Row 5 (SPACE BAR and icons row)
    {
        if ((iTx >= 0) && (iTx < 35))  return MENU_CHOICE_MENU;
        else if ((iTx >= 35)  && (iTx <  67))  {kbd_key = KBD_SHIFT; shift_key = 1;}
        else if ((iTx >= 67)  && (iTx <  99))  {kbd_key = KBD_CTRL; ctrl_key = 1;}
        else if ((iTx >= 99)  && (iTx < 203))  kbd_key = KBD_SPACE;
        else if ((iTx >= 203) && (iTx < 255))  kbd_key = KBD_ENTER;
    }

    DisplayStatusLine();

    return MENU_CHOICE_NONE;
}


u8 __attribute__((noinline)) handle_meta_key(u8 meta_key)
{
    switch (meta_key)
    {
        case MENU_CHOICE_RESET_GAME:
            SoundPause();
            // Ask for verification
            if (showMessage("DO YOU REALLY WANT TO", "RESET THE CURRENT GAME ?") == ID_SHM_YES)
            {
                ResetMicroComputer();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_END_GAME:
              SoundPause();
              //  Ask for verification
              if  (showMessage("DO YOU REALLY WANT TO","QUIT THE CURRENT GAME ?") == ID_SHM_YES)
              {
                  memset((u8*)0x06000000, 0x00, 0x20000);    // Reset VRAM to 0x00 to clear any potential display garbage on way out
                  return 1;
              }
              BottomScreenKeyboard();
              DisplayStatusLine();
              SoundUnPause();
            break;

        case MENU_CHOICE_SAVE_GAME:
            SoundPause();
            if  (showMessage("DO YOU REALLY WANT TO","SAVE GAME STATE ?") == ID_SHM_YES)
            {
                MicroSaveState();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_LOAD_GAME:
            SoundPause();
            if (showMessage("DO YOU REALLY WANT TO","LOAD GAME STATE ?") == ID_SHM_YES)
            {
                MicroLoadState();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_DEFINE_KEYS:
            SoundPause();
            MicroDSChangeKeymap();
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_GAME_OPTION:
            SoundPause();
            MicroDSGameOptions(0);
            BottomScreenKeyboard();
            SoundUnPause();
            break;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// Slide-n-Glide D-pad keeps moving in the last known direction for a few more
// frames to help make those hairpin turns up and off ladders much easier...
// ----------------------------------------------------------------------------
u8 slide_n_glide_key_up     __attribute__((section(".dtcm"))) = 0;
u8 slide_n_glide_key_down   __attribute__((section(".dtcm"))) = 0;
u8 slide_n_glide_key_left   __attribute__((section(".dtcm"))) = 0;
u8 slide_n_glide_key_right  __attribute__((section(".dtcm"))) = 0;

s8 digital_offset_x         __attribute__((section(".dtcm"))) = 0;
s8 digital_offset_y         __attribute__((section(".dtcm"))) = 0;

// ------------------------------------------------------------------------
// The main emulation loop is here... call into the Z80 and render frame
// ------------------------------------------------------------------------
ITCM_CODE void MicroDS_main(void)
{
  static u8 dampenClick = 0;
  u16 iTx,  iTy;
  u8 meta_key = 0;

  // Setup the debug buffer for DSi use
  debug_init();

  // Get the Emulator ready
  MC10Init(gpFic[ucGameAct].szName);

  MC10SetPalette();
  RunMicroComputer();

  // Frame-to-frame timing...
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

  // Once/second timing...
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;

  newStreamSampleRate();

  // Force the sound engine to turn on when we start emulation
  bStartSoundEngine = 10;

  // -----------------------------------------------------------
  // Stay in this loop running the game until the user exits...
  // -----------------------------------------------------------
  while(1)
  {
    // Take a tour of the Z80 counter and display the screen if necessary
    if (micro_run())
    {
        // If we've been asked to start the sound engine, rock-and-roll!
        if (bStartSoundEngine)
        {
              if (--bStartSoundEngine == 0) SoundUnPause();
        }

        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   //  1000MS (1 sec)
        {
            char szChai[4];
            
            read_cassette_counter = 0;

            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
            emuFps = emuActFrames;
            if (myGlobalConfig.showFPS)
            {
                if (emuFps/100) szChai[0] = '0' + emuFps/100;
                else szChai[0] = ' ';
                szChai[1] = '0' + (emuFps%100) / 10;
                szChai[2] = '0' + (emuFps%100) % 10;
                szChai[3] = 0;
                DSPrint(0,0,6,szChai);
            }
            DisplayStatusLine();
            emuActFrames = 0;
        }
        emuActFrames++;

        // ---------------------------------------------
        // We support Tandy MC-10 at 60Hz NTSC only
        // ---------------------------------------------
        if (++timingFrames == 60)
        {
            TIMER2_CR=0;
            TIMER2_DATA=0;
            TIMER2_CR=TIMER_ENABLE | TIMER_DIV_1024;
            timingFrames = 0;
        }

        // ----------------------------------------------------------------------
        // 32,728.5 ticks of TIMER2 = 1 second
        // 1 frame = 1/50 or 655 ticks of TIMER2
        //
        // This is how we time frame-to frame to keep the game running at 50FPS
        // ----------------------------------------------------------------------
        while (TIMER2_DATA < (GAME_SPEED_NTSC[myConfig.gameSpeed] *(timingFrames+1)))
        {
            if (myGlobalConfig.showFPS == 2) break;   // If Full Speed, break out...
            if (tape_motor && tape_speedup) break; // If running TAPE go full speed
        }


        // If the Z80 Debugger is enabled, call it
        if (myGlobalConfig.debugger)
        {
            ShowDebugger();
        }

        uint16_t keys_current = keysCurrent();

        // --------------------------------------------------------------------------
        // The KEY_START NDS key will auto-load the current .C10 cassette program.
        // The KEY_SELECT NDS key will issue the 'RUN' command (useful after CLOAD).
        // --------------------------------------------------------------------------
        if (myConfig.autoLoad && (BufferedKeysReadIdx == BufferedKeysWriteIdx))
        {
            // START key is special...
            if (keys_current & KEY_START)
            {
                BufferKey(KBD_C);    // C
                BufferKey(KBD_L);    // L
                BufferKey(KBD_O);    // O
                BufferKey(KBD_A);    // A
                BufferKey(KBD_D);    // D
                if (myConfig.autoLoad == AUTOLOAD_CLOADM)
                {
                    BufferKey(KBD_M);    // M
                    BufferKey(KBD_COLON);// :

                    BufferKey(KBD_E);    // E
                    BufferKey(KBD_X);    // X
                    BufferKey(KBD_E);    // E
                    BufferKey(KBD_C);    // C
                }

                BufferKey(KBD_ENTER); // ENTER
                BufferKey(255);       // END
            }
            
            // SELECT key is special...
            if (keys_current & KEY_SELECT)
            {
                BufferKey(KBD_R);     // R
                BufferKey(KBD_U);     // U
                BufferKey(KBD_N);     // N
                BufferKey(KBD_ENTER); // ENTER
                BufferKey(255);       // END
            }
        }


      // --------------------------------------------------------------
      // Hold the key press for a brief instant... To allow the
      // emulated CPU to 'see' the key briefly... Good enough.
      // --------------------------------------------------------------
      if (BufferedKeysReadIdx == BufferedKeysWriteIdx)
      {
          if (key_debounce > 0) key_debounce--;
          else
          {
              // -----------------------------------------------------------
              // This is where we accumualte the keys pressed... up to 12!
              // -----------------------------------------------------------
              kbd_keys_pressed = 0;
              memset(kbd_keys, 0x00, sizeof(kbd_keys));
              kbd_key = 0;

              // ------------------------------------------
              // Handle any screen touch events
              // ------------------------------------------
              if  (keys_current & KEY_TOUCH)
              {
                  // ------------------------------------------------------------------------------------------------
                  // Just a tiny bit of touch debounce so ensure touch screen is pressed for a fraction of a second.
                  // ------------------------------------------------------------------------------------------------
                  if (++touch_debounce > 1)
                  {
                    touchPosition touch;
                    touchRead(&touch);
                    iTx = touch.px;
                    iTy = touch.py;

                    // ------------------------------------------------------------
                    // Test the touchscreen for various full keyboard handlers...
                    // ------------------------------------------------------------
                    meta_key = handle_keyboard_press(iTx, iTy);

                    // If the special menu key indicates we should show the choice menu, do so here...
                    if (meta_key == MENU_CHOICE_MENU)
                    {
                        meta_key = MiniMenu();
                    }

                    // -------------------------------------------------------------------
                    // If one of the special meta keys was picked, we handle that here...
                    // -------------------------------------------------------------------
                    if (handle_meta_key(meta_key)) return;

                    if (++dampenClick > 0)  // Make sure the key is pressed for an appreciable amount of time...
                    {
                        if (kbd_key != 0)
                        {
                            if (shift_key && (kbd_key != KBD_SHIFT)) // First non-shift key pairs with shift
                            {
                                kbd_keys[kbd_keys_pressed++] = KBD_SHIFT; // Shift
                                shift_key = 0;
                            }
                            if (ctrl_key && (kbd_key != KBD_CTRL)) // First non-ctrl key pairs with ctrl
                            {
                                kbd_keys[kbd_keys_pressed++] = KBD_CTRL; // Control
                                ctrl_key = 0;
                            }
                            kbd_keys[kbd_keys_pressed++] = kbd_key;
                            key_debounce = 5;
                            if (last_kbd_key == 0)
                            {
                                 mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
                            }
                            last_kbd_key = kbd_key;
                        }
                    }
                  }
              } //  SCR_TOUCH
              else
              {
                touch_debounce = 0;
                dampenClick = 0;
                last_kbd_key = 0;
              }

              // ---------------------------------------------------------------------------
              //  Test DS keypresses (ABXY, L/R) and map to corresponding MC-10 keys
              // ---------------------------------------------------------------------------
              nds_key  = keys_current;     // Get any current keys pressed on the NDS

              // -----------------------------------------
              // Check various key combinations first...
              // -----------------------------------------
              if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_X))
              {
                    lcdSwap();
                    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
              }
              else if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_Y))
              {
                    DSPrint(2,0,0,"SNAPSHOT");
                    screenshot();
                    debug_save();
                    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                    DSPrint(2,0,0,"        ");
              }
              else if  (nds_key & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y))
              {
                  if (myConfig.dpad == DPAD_SLIDE_N_GLIDE) // CHUCKIE-EGG Style... hold left/right or up/down for a few frames
                  {
                        if (nds_key & KEY_UP)
                        {
                            slide_n_glide_key_up    = 12;
                            slide_n_glide_key_down  = 0;
                        }
                        if (nds_key & KEY_DOWN)
                        {
                            slide_n_glide_key_down  = 12;
                            slide_n_glide_key_up    = 0;
                        }
                        if (nds_key & KEY_LEFT)
                        {
                            slide_n_glide_key_left  = 12;
                            slide_n_glide_key_right = 0;
                        }
                        if (nds_key & KEY_RIGHT)
                        {
                            slide_n_glide_key_right = 12;
                            slide_n_glide_key_left  = 0;
                        }

                        if (slide_n_glide_key_up)
                        {
                            slide_n_glide_key_up--;
                            nds_key |= KEY_UP;
                        }

                        if (slide_n_glide_key_down)
                        {
                            slide_n_glide_key_down--;
                            nds_key |= KEY_DOWN;
                        }

                        if (slide_n_glide_key_left)
                        {
                            slide_n_glide_key_left--;
                            nds_key |= KEY_LEFT;
                        }

                        if (slide_n_glide_key_right)
                        {
                            slide_n_glide_key_right--;
                            nds_key |= KEY_RIGHT;
                        }
                  }
                  else if (myConfig.dpad == DPAD_DIAGONALS)
                  {
                           if (nds_key & KEY_UP)    nds_key |= KEY_RIGHT;  // UP-RIGHT
                      else if (nds_key & KEY_DOWN)  nds_key |= KEY_LEFT;   // DOWN-LEFT
                      else if (nds_key & KEY_LEFT)  nds_key |= KEY_UP;     // UP-LEFT
                      else if (nds_key & KEY_RIGHT) nds_key |= KEY_DOWN;   // DOWN-RIGHT
                  }

                  // ------------------------------------------------------------------------------------------------------
                  // There are 10 NDS buttons that can be mapped (D-Pad, XYAB, L/R) - we allow mapping these to MC-10 keys.
                  // ------------------------------------------------------------------------------------------------------
                  for (u8 i=0; i<10; i++)
                  {
                      if (nds_key & NDS_keyMap[i])
                      {
                          // This is a keyboard maping... handle that here... just set the appopriate kbd_key
                          kbd_key = myConfig.keymap[i];
                          kbd_keys[kbd_keys_pressed++] = kbd_key;
                      }
                  }
              }
              else // No NDS keys pressed...
              {
                  if (slide_n_glide_key_up)    slide_n_glide_key_up--;
                  if (slide_n_glide_key_down)  slide_n_glide_key_down--;
                  if (slide_n_glide_key_left)  slide_n_glide_key_left--;
                  if (slide_n_glide_key_right) slide_n_glide_key_right--;
                  last_mapped_key = 0;
              }
          }
      }
      else
      {
          // ------------------------------------------------------------------------------------------
          // Finally, check if there are any buffered keys that need to go into the keyboard handling.
          // ------------------------------------------------------------------------------------------
          ProcessBufferedKeys();
      }
    }
  }
}


// ----------------------------------------------------------------------------------------
// We steal 256K of the VRAM to hold a shadow copy of the ROM cart for fast swap...
// ----------------------------------------------------------------------------------------
void useVRAM(void)
{
    vramSetBankB(VRAM_B_LCD);        // 128K VRAM used for snapshot DCAP buffer - but could be repurposed during emulation ...
    vramSetBankD(VRAM_D_LCD);        // Not using this for video but 128K of faster RAM always useful!  Mapped at 0x06860000 -   Unused - reserved for future use
    vramSetBankE(VRAM_E_LCD);        // Not using this for video but 64K of faster RAM always useful!   Mapped at 0x06880000 -   Unused - reserved for future use
    vramSetBankF(VRAM_F_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06890000 -   Unused - reserved for future use
    vramSetBankG(VRAM_G_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06894000 -   Unused - reserved for future use
    vramSetBankH(VRAM_H_LCD);        // Not using this for video but 32K of faster RAM always useful!   Mapped at 0x06898000 -   Unused - reserved for future use
    vramSetBankI(VRAM_I_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x068A0000 -   Unused - reserved for future use
}

/*********************************************************************************
 * Init DS Emulator - setup VRAM banks and background screen rendering banks
 ********************************************************************************/
void MicroDSInit(void)
{
    //  Init graphic mode (bitmap mode)
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE  | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
   
    //  Stop blending effect of intro
    REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;
   
    //  Render the top screen
    bg0 = bgInit(0, BgType_Text8bpp,  BgSize_T_256x512, 31,0);
    bg1 = bgInit(1, BgType_Text8bpp,  BgSize_T_256x512, 29,0);
    bgSetPriority(bg0,1);bgSetPriority(bg1,0);
   
    decompress(splashTiles,  bgGetGfxPtr(bg0), LZ77Vram);
    decompress(splashMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
    dmaCopy((void*) splashPal,(void*)  BG_PALETTE,256*2);

    unsigned  short dmaVal =*(bgGetMapPtr(bg0)+51*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1),32*24*2);
   
    // Put up the options screen
    BottomScreenOptions();
   
    //  Find the files
    MicroDSFindFiles(0);
}

void BottomScreenOptions(void)
{
    swiWaitForVBlank();

    if (bottom_screen != 1)
    {
        // ---------------------------------------------------
        // Put up the options select screen background...
        // ---------------------------------------------------
        bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
        bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 29,0);
        bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

        decompress(mainmenuTiles, bgGetGfxPtr(bg0b), LZ77Vram);
        decompress(mainmenuMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
        dmaCopy((void*) mainmenuPal,(void*) BG_PALETTE_SUB,256*2);

        unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
        dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    }
    else // Just clear the screen
    {
        for (u8 i=0; i<23; i++)  DSPrint(0,i,0,"                                ");
    }

    bottom_screen = 1;
}

// ---------------------------------------------------------------------------
// Setup the bottom screen - mostly for menu, high scores, options, etc.
// ---------------------------------------------------------------------------
void BottomScreenKeyboard(void)
{
    swiWaitForVBlank();

    //  Init bottom screen for Tandy MC-10 Virtual Keyboard
    if (myGlobalConfig.debugger)
    {
        decompress(debug_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
        decompress(debug_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
        dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
        dmaCopy((void*) debug_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else
    {
        decompress(mc10_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
        decompress(mc10_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
        dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
        dmaCopy((void*) mc10_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }

    unsigned  short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);

    bottom_screen = 2;

    DisplayStatusLine();
}


/*********************************************************************************
 * Init CPU for the current game
 ********************************************************************************/
void MicroDSInitCPU(void)
{
    // -----------------------------------------------
    // Init bottom screen do display the Keyboard
    // -----------------------------------------------
    BottomScreenKeyboard();
}

// -------------------------------------------------------------
// Only used for basic timing of splash screen fade-out
// -------------------------------------------------------------
ITCM_CODE void irqVBlank(void)
{
    // Manage time
    vusCptVBL++;
}

// ------------------------------
// Look for the BIOS/BASIC files
// ------------------------------
void LoadBIOSFiles(void)
{
    int size = 0;

    // -----------------------------------------------------
    // We will look for the various needed BASIC/BIOS roms
    // -----------------------------------------------------
    bBIOS_found = false;
    bMCX_found = false;

    memset(MC10BASIC,   0xFF, sizeof(MC10BASIC));
    memset(MCXBASIC,    0xFF, sizeof(MCXBASIC));

    // ----------------------------------------------------
    // Try to load the MC-10 BIOS/BASIC file
    // ----------------------------------------------------
               size = ReadFileCarefully("mc10.rom",               MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/mc10.rom",    MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/mc10.rom",    MC10BASIC, 0x2000, 0);

    if (!size) size = ReadFileCarefully("mc10.bin",               MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/mc10.bin",    MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/mc10.bin",    MC10BASIC, 0x2000, 0);

    if (!size) size = ReadFileCarefully("mc-10.rom",               MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/mc-10.rom",    MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/mc-10.rom",    MC10BASIC, 0x2000, 0);

    if (!size) size = ReadFileCarefully("mc-10.bin",               MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/mc-10.bin",    MC10BASIC, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/mc-10.bin",    MC10BASIC, 0x2000, 0);
        
    if (size) bBIOS_found = true;
    
    // And the optional MCX BASIC rom file
               size = ReadFileCarefully("mcx.bin",                 MCXBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/mcx.bin",      MCXBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/mcx.bin",      MCXBASIC, 0x4000, 0);
    
    if (!size) size = ReadFileCarefully("mcx.rom",                 MCXBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/mcx.rom",      MCXBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/mcx.rom",      MCXBASIC, 0x4000, 0);
    
    if (size) bMCX_found = true;
}

/************************************************************************************
 * Program entry point - check if an argument has been passed in probably from TWL++
 ***********************************************************************************/
int main(int argc, char **argv)
{
    //  Init sound
    consoleDemoInit();
   
    if  (!fatInitDefault())
    {
       iprintf("Unable to initialize libfat!\n");
       return -1;
    }
   
    lcdMainOnTop();
   
    //  Init timer for frame management
    TIMER2_DATA=0;
    TIMER2_CR=TIMER_ENABLE|TIMER_DIV_1024;
    dsInstallSoundEmuFIFO();
   
    // -----------------------------------------------------------------
    // And do an initial load of configuration... We'll match it up
    // with the game that was selected later...
    // -----------------------------------------------------------------
    LoadConfig();
   
    //  Show the fade-away intro logo...
    intro_logo();
   
    SetYtrigger(190); //trigger 2 lines before vblank
   
    irqSet(IRQ_VBLANK,  irqVBlank);
    irqEnable(IRQ_VBLANK);
   
    // -----------------------------------------------------------------
    // Grab the BIOS before we try to switch any directories around...
    // -----------------------------------------------------------------
    useVRAM();
    LoadBIOSFiles();
   
    //  Handle command line argument... mostly for TWL++
    if  (argc > 1)
    {
        //  We want to start in the directory where the file is being launched...
        if  (strchr(argv[1], '/') != NULL)
        {
            static char  path[128];
            strcpy(path,  argv[1]);
            char  *ptr = &path[strlen(path)-1];
            while (*ptr !=  '/') ptr--;
            ptr++;
            strcpy(cmd_line_file,  ptr);
            *ptr=0;
            chdir(path);
        }
        else
        {
            strcpy(cmd_line_file,  argv[1]);
        }
    }
    else
    {
        cmd_line_file[0]=0; // No file passed on command line...
   
        chdir("/roms");       // Try to start in roms area... doesn't matter if it fails
        chdir("mc10");        // And try to start in the mc10 sub-dir
        chdir("mc-10");       // And try to start in the mc10 sub-dir
    }
   
    SoundPause();
   
    srand(time(NULL));
   
    //  ------------------------------------------------------------
    //  We run this loop forever until game exit is selected...
    //  ------------------------------------------------------------
    while(1)
    {
      MicroDSInit();
   
      // ---------------------------------------------------------------
      // Let the user know what BIOS files were found - the only BIOS
      // that must exist is 48.rom or else the show is off...
      // ---------------------------------------------------------------
      if (!bBIOS_found)
      {
          DSPrint(2,10,0," ERROR: MC10.ROM BASIC/BIOS ");
          DSPrint(2,12,0," NOT FOUND. PLACE THESE IN  ");
          DSPrint(2,14,0," /ROMS/BIOS OR WITH EMULATOR");
          while(1) ;  // We're done... Need a bios to run this emulator
      }
   
      while(1)
      {
        SoundPause();
        //  Choose option
        if  (cmd_line_file[0] != 0)
        {
            ucGameChoice=0;
            ucGameAct=0;
            strcpy(gpFic[ucGameAct].szName, cmd_line_file);
            cmd_line_file[0] = 0;    // No more initial file...
            ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
        }
        else
        {
            MicroDSChangeOptions();
        }
   
        //  Run Machine
        MicroDSInitCPU();
        MicroDS_main();
      }
    }
    
    return(0);
}


// -----------------------------------------------------------------------
// The code below is a handy set of debug tools that allows us to
// write printf() like strings out to a file. Basically we accumulate
// the strings into a large RAM buffer and then when the L+R shoulder
// buttons are pressed and held, we will snapshot out the debug.log file.
// The DS-Lite only gets a small 16K debug buffer but the DSi gets 2MB!
// -----------------------------------------------------------------------

#define MAX_DPRINTF_STR_SIZE  256
u32     MAX_DEBUG_BUF_SIZE  = 0;

char *debug_buffer = 0;
u32  debug_len = 0;
extern char szName[]; // Reuse buffer which has no other in-game use

void debug_init()
{
    if (!debug_buffer)
    {
        if (isDSiMode())
        {
            MAX_DEBUG_BUF_SIZE = (1024*1024*2); // 2MB!!
            debug_buffer = malloc(MAX_DEBUG_BUF_SIZE);
        }
        else
        {
            MAX_DEBUG_BUF_SIZE = (1024*16);     // 16K only
            debug_buffer = malloc(MAX_DEBUG_BUF_SIZE);
        }
    }
    memset(debug_buffer, 0x00, MAX_DEBUG_BUF_SIZE);
    debug_len = 0;
}

void debug_printf(const char * str, ...)
{
    va_list ap = {0};

    va_start(ap, str);
    vsnprintf(szName, MAX_DPRINTF_STR_SIZE, str, ap);
    va_end(ap);

    strcat(debug_buffer, szName);
    debug_len += strlen(szName);
}

void debug_save()
{
    if (debug_len > 0) // Only if we have debug data to write...
    {
        FILE *fp = fopen("debug.log", "w");
        if (fp)
        {
            fwrite(debug_buffer, 1, debug_len, fp);
            fclose(fp);
        }
    }
}

// End of file
