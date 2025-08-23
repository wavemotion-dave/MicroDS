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

#ifndef _MICRODS_H_
#define _MICRODS_H_

#include <nds.h>
#include <string.h>

extern unsigned int debug[0x10];
extern u32 DX, DY;

// These are the various special icons/menu operations
#define MENU_CHOICE_NONE        0x00
#define MENU_CHOICE_RESET_GAME  0x01
#define MENU_CHOICE_END_GAME    0x02
#define MENU_CHOICE_SAVE_GAME   0x03
#define MENU_CHOICE_LOAD_GAME   0x04
#define MENU_CHOICE_DEFINE_KEYS 0x05
#define MENU_CHOICE_GAME_OPTION 0x06
#define MENU_CHOICE_TAPE_REWIND 0x07
#define MENU_CHOICE_TAPE_STOP   0x08
#define MENU_CHOICE_MENU        0xFF        // Special brings up a mini-menu of choices

#define MAX_KEY_OPTIONS     49

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

extern u8 micro_mode;
extern u8 kbd_keys_pressed;
extern u8 kbd_keys[12];
extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;
extern char initial_file[];
extern char initial_path[];
extern u16 nds_key;
extern u8  kbd_key;
extern u16 vusCptVBL;
extern u16 keyCoresp[MAX_KEY_OPTIONS];
extern u16 NDS_keyMap[];
extern u8 soundEmuPause;
extern int bg0, bg1, bg0b, bg1b;
extern u32 last_file_size;
extern u8 bottom_screen;
extern u8 shift_key, ctrl_key;
extern u8 bMCX_found;
extern u8 bALICE_found;

extern void BottomScreenOptions(void);
extern void BottomScreenKeyboard(void);
extern void PauseSound(void);
extern void UnPauseSound(void);
extern void ResetStatusFlags(void);
extern void ReadFileCRCAndConfig(void);
extern void DisplayStatusLine(void);
extern void ResetMicroComputer(void);
extern void processDirectAudio(void);
extern void newStreamSampleRate(void);
extern void debug_init();
extern void debug_save();
extern void debug_printf(const char * str, ...);

#endif // _MICRODS_H_
