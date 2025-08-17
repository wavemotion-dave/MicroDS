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

#ifndef _MICRO_UTILS_H_
#define _MICRO_UTILS_H_

#include <nds.h>
#include "MicroDS.h"
#include "cpu.h"

#define MAX_FILES                   1048
#define MAX_FILENAME_LEN            160
#define MAX_FILE_SIZE               (64*1024) // 64K is big enough for any .C10 file

#define MAX_CONFIGS                 1000
#define CONFIG_VERSION              0x0003

#define MICRO_FILE                  0x01
#define DIRECTORY                   0x02

#define ID_SHM_CANCEL               0x00
#define ID_SHM_YES                  0x01
#define ID_SHM_NO                   0x02

#define DPAD_NORMAL                 0
#define DPAD_SLIDE_N_GLIDE          1
#define DPAD_DIAGONALS              2

#define AUTOLOAD_NONE               0
#define AUTOLOAD_CLOAD              1
#define AUTOLOAD_CLOADM             2

#define MACHINE_20K                 0
#define MACHINE_32K                 1

extern unsigned char MC10BASIC[0x2000];
extern unsigned char MCXBASIC[0x4000];

extern char last_path[MAX_FILENAME_LEN];
extern char last_file[MAX_FILENAME_LEN];

extern u32 micro_line;
extern u8  micro_special_key;
extern u32 last_file_size;
extern u32 micro_scanline_counter;
extern u32 file_size;
extern u32 tape_pos;
extern u8  tape_motor;
extern u8  tape_speedup;

typedef struct {
  char szName[MAX_FILENAME_LEN+1];
  u8 uType;
  u32 uCrc;
} FIMicro;

typedef u16 word;

struct __attribute__((__packed__)) GlobalConfig_t
{
    u16 config_ver;
    u32 bios_checksums;
    char szLastFile[MAX_FILENAME_LEN+1];
    char szLastPath[MAX_FILENAME_LEN+1];
    char reserved1[MAX_FILENAME_LEN+1];
    char reserved2[MAX_FILENAME_LEN+1];
    u8  showFPS;
    u8  defMachine;
    u8  global_01;
    u8  global_02;
    u8  global_03;
    u8  global_04;
    u8  global_05;
    u8  global_06;
    u8  global_07;
    u8  global_08;
    u8  global_09;
    u8  global_10;
    u8  global_11;
    u8  global_12;
    u8  debugger;
    u32 config_checksum;
};

struct __attribute__((__packed__)) Config_t
{
    u32 game_crc;
    u8  keymap[12];
    u8  machine;
    u8  autoLoad;
    u8  gameSpeed;
    u8  autoFire;
    u8  dpad;
    u8  reserved1;
    u8  reserved2;
    u8  reserved3;
    u8  reserved4;
    u8  reserved5;
    u8  reserved6;
    u8  reserved7;
    u8  reserved8;
    u8  reserved9;
    u8  reserved10;
    u8  reserved11;
};

typedef enum
{
  KBD_NONE = 0,
  KBD_A,
  KBD_B,
  KBD_C,
  KBD_D,
  KBD_E,
  KBD_F,
  KBD_G,
  KBD_H,
  KBD_I,
  KBD_J,
  KBD_K,
  KBD_L,
  KBD_M,
  KBD_N,
  KBD_O,
  KBD_P,
  KBD_Q,
  KBD_R,
  KBD_S,
  KBD_T,
  KBD_U,
  KBD_V,
  KBD_W,
  KBD_X,
  KBD_Y,
  KBD_Z,
  KBD_1,
  KBD_2,
  KBD_3,
  KBD_4,
  KBD_5,
  KBD_6,
  KBD_7,
  KBD_8,
  KBD_9,
  KBD_0,
  KBD_ATSIGN,
  KBD_COLON,
  KBD_SEMI,
  KBD_COMMA,
  KBD_DASH,
  KBD_PERIOD,
  KBD_SLASH,
  KBD_ENTER,  
  KBD_SPACE,
  KBD_SHIFT,
  KBD_CTRL,
  KBD_BREAK
} kbd_t;

extern struct Config_t       myConfig;
extern struct GlobalConfig_t myGlobalConfig;

extern u32 file_crc;
extern u8 bFirstTime;

extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;

extern u8 TapeBuffer[MAX_FILE_SIZE];

extern FIMicro gpFic[MAX_FILES];
extern short int ucGameAct;
extern short int ucGameChoice;

extern void LoadConfig(void);
extern u8   showMessage(char *szCh1, char *szCh2);
extern void MicroDSFindFiles(u8 bDiskOnly);
extern void MicroDSChangeOptions(void);
extern void MicroDSGameOptions(bool bIsGlobal);
extern void DSPrint(int iX,int iY,int iScr,char *szMessage);
extern u32  crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern u8   MicroDSLoadFile(u8 bDiskOnly);
extern void DisplayFileName(void);
extern u32  ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset);
extern u8   loadgame(const char *path);
extern u8   MC10Init(char *szGame);
extern void MC10SetPalette(void);
extern void RunMicroComputer(void);
extern void micro_reset(void);
extern u32  micro_run(void);
extern void getfile_crc(const char *path);
extern void MicroLoadState();
extern void MicroSaveState();
extern void intro_logo(void);
extern void BufferKey(u8 key);
extern void ProcessBufferedKeys(void);
extern void MicroDSChangeKeymap(void);

#endif // _MICRO_UTILS_H_
