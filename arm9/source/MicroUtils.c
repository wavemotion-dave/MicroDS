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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <maxmod9.h>

#include "MicroDS.h"
#include "MicroUtils.h"
#include "splash.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "splash_bot.h"
#include "tape.h"
#include "CRC32.h"
#include "printf.h"

short int   fileCount=0;
short int   ucGameAct=0;
short int   ucGameChoice = -1;
FIMicro     gpFic[MAX_FILES];
char        szName[256];
char        szName2[40];
char        szFile[256];
u32         file_size = 0;
char        strBuf[40];

struct Config_t         AllConfigs[MAX_CONFIGS];
struct Config_t         myConfig __attribute((aligned(4))) __attribute__((section(".dtcm")));
struct GlobalConfig_t   myGlobalConfig;

u16 *pVidFlipBuf  = (u16*) (0x06000000);    // Video flipping buffer

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

u8 option_table=0;

const char szKeyName[MAX_KEY_OPTIONS][18] = {
  "KEY NONE",   // 0
  "KEYBOARD A", // 1
  "KEYBOARD B",
  "KEYBOARD C",
  "KEYBOARD D",
  "KEYBOARD E", // 5
  "KEYBOARD F",
  "KEYBOARD G",
  "KEYBOARD H",
  "KEYBOARD I",
  "KEYBOARD J", // 10
  "KEYBOARD K",
  "KEYBOARD L",
  "KEYBOARD M",
  "KEYBOARD N",
  "KEYBOARD O", // 15
  "KEYBOARD P",
  "KEYBOARD Q",
  "KEYBOARD R",
  "KEYBOARD S",
  "KEYBOARD T", // 20
  "KEYBOARD U",
  "KEYBOARD V",
  "KEYBOARD W",
  "KEYBOARD X",
  "KEYBOARD Y", // 25
  "KEYBOARD Z",

  "KEYBOARD 1", // 27
  "KEYBOARD 2",
  "KEYBOARD 3",
  "KEYBOARD 4", // 30
  "KEYBOARD 5",
  "KEYBOARD 6",
  "KEYBOARD 7",
  "KEYBOARD 8",
  "KEYBOARD 9", // 35
  "KEYBOARD 0",

  "KEYBOARD ATSIGN", // 37
  "KEYBOARD COLON",  // 38
  "KEYBOARD SEMI",   // 39
  "KEYBOARD COMMA",  // 40
  "KEYBOARD DASH",   // 41
  "KEYBOARD PERIOD", // 42
  "KEYBOARD SLASH",  // 43
  "KEYBOARD ENTER",  // 44
  "KEYBOARD SPACE",  // 45
  "KEYBOARD SHIFT",  // 46
  "KEYBOARD CTRL"   ,// 47
  "KEYBOARD BREAK",  // 48
};


/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2)
{
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;

  BottomScreenOptions();

  DSPrint(16-strlen(szCh1)/2,10,6,szCh1);
  DSPrint(16-strlen(szCh2)/2,12,6,szCh2);
  DSPrint(8,14,6,("> YES <"));
  DSPrint(20,14,6,("  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL)
  {
    WAITVBL;
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
          ucGauS = 1;
          if (ucCho == ID_SHM_YES) {
            uRet = ucCho;
          }
          else {
            ucCho  = ID_SHM_YES;
          }
        }
      }
      else
        ucGauS = 0;
      if ( (iTx>20*8) && (iTx<20*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucDroS) {
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
          ucDroS = 1;
          if (ucCho == ID_SHM_NO) {
            uRet = ucCho;
          }
          else {
            ucCho = ID_SHM_NO;
          }
        }
      }
      else
        ucDroS = 0;
    }
    else {
      ucDroS = 0;
      ucGauS = 0;
    }

    if (keysCurrent() & KEY_LEFT){
      if (!ucGau) {
        ucGau = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucGau = 0;
    }
    if (keysCurrent() & KEY_RIGHT) {
      if (!ucDro) {
        ucDro = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho  = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      }
    }
    else {
      ucDro = 0;
    }
    if (keysCurrent() & KEY_A) {
      uRet = ucCho;
    }
  }
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  BottomScreenKeyboard();

  return uRet;
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
void dsDisplayFiles(u16 NoDebGame, u8 ucSel)
{
  u16 ucBcl,ucGame;
  u8 maxLen;

  DSPrint(31,6,0,(NoDebGame>0 ? "<" : " "));
  DSPrint(31,22,0,(NoDebGame+14<fileCount ? ">" : " "));

  for (ucBcl=0;ucBcl<17; ucBcl++)
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < fileCount)
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>30) szName[30]='\0';
      if (gpFic[ucGame].uType == DIRECTORY)
      {
        szName[28] = 0; // Needs to be 2 chars shorter with brackets
        sprintf(szName2, "[%s]",szName);
        sprintf(szName,"%-30s",szName2);
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else
      {
        sprintf(szName,"%-30s",strupr(szName));
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        DSPrint(1,6+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                              ");
    }
  }
}


// -------------------------------------------------------------------------
// Standard qsort routine for the games - we sort all directory
// listings first and then a case-insenstive sort of all games.
// -------------------------------------------------------------------------
int Filescmp (const void *c1, const void *c2)
{
  FIMicro *p1 = (FIMicro *) c1;
  FIMicro *p2 = (FIMicro *) c2;

  if (p1->szName[0] == '.' && p2->szName[0] != '.')
      return -1;
  if (p2->szName[0] == '.' && p1->szName[0] != '.')
      return 1;
  if ((p1->uType == DIRECTORY) && !(p2->uType == DIRECTORY))
      return -1;
  if ((p2->uType == DIRECTORY) && !(p1->uType == DIRECTORY))
      return 1;
  return strcasecmp (p1->szName, p2->szName);
}

/*********************************************************************************
 * Find game/program files available - sort them for display.
 ********************************************************************************/
void MicroDSFindFiles(void)
{
  u32 uNbFile;
  DIR *dir;
  struct dirent *pent;

  uNbFile=0;
  fileCount=0;

  dir = opendir(".");
  while (((pent=readdir(dir))!=NULL) && (uNbFile<MAX_FILES))
  {
    strcpy(szFile,pent->d_name);

    if(pent->d_type == DT_DIR)
    {
      if (!((szFile[0] == '.') && (strlen(szFile) == 1)))
      {
        // Do not include the [sav] and [pok] directories
        if ((strcasecmp(szFile, "sav") != 0) && (strcasecmp(szFile, "pok") != 0))
        {
            strcpy(gpFic[uNbFile].szName,szFile);
            gpFic[uNbFile].uType = DIRECTORY;
            uNbFile++;
            fileCount++;
        }
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_FILENAME_LEN-4)) && (szFile[0] != '.') && (szFile[0] != '_'))  // For MAC don't allow files starting with an underscore
      {
        if ( (strcasecmp(strrchr(szFile, '.'), ".c10") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = MICRO_FILE;
          uNbFile++;
          fileCount++;
        }
      }
    }
  }
  closedir(dir);

  // ----------------------------------------------
  // If we found any files, go sort the list...
  // ----------------------------------------------
  if (fileCount)
  {
    qsort (gpFic, fileCount, sizeof(FIMicro), Filescmp);
  }
}

// ----------------------------------------------------------------
// Let the user select a new game (rom) file and load it up!
// ----------------------------------------------------------------
u8 MicroDSLoadFile(void)
{
  bool bDone=false;
  u16 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00, romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s16 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);

  BottomScreenOptions();

  MicroDSFindFiles();

  ucGameChoice = -1;

  nbRomPerPage = (fileCount>=17 ? 17 : fileCount);
  uNbRSPage = (fileCount>=5 ? 5 : fileCount);

  if (ucGameAct>fileCount-nbRomPerPage)
  {
    firstRomDisplay=fileCount-nbRomPerPage;
    romSelected=ucGameAct-fileCount+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucGameAct;
    romSelected=0;
  }

  if (romSelected >= fileCount) romSelected = 0; // Just start at the top

  dsDisplayFiles(firstRomDisplay,romSelected);

  // -----------------------------------------------------
  // Until the user selects a file or exits the menu...
  // -----------------------------------------------------
  while (!bDone)
  {
    if (keysCurrent() & KEY_UP)
    {
      if (!ucHaut)
      {
        ucGameAct = (ucGameAct>0 ? ucGameAct-1 : fileCount-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=fileCount-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {

        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else
    {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        ucGameAct = (ucGameAct< fileCount-1 ? ucGameAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<fileCount-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_RIGHT)
    {
      if (!ucSBas)
      {
        ucGameAct = (ucGameAct< fileCount-nbRomPerPage ? ucGameAct+nbRomPerPage : fileCount-nbRomPerPage);
        if (firstRomDisplay<fileCount-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = fileCount-nbRomPerPage; }
        if (ucGameAct == fileCount-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSBas = 0;
    }

    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_LEFT)
    {
      if (!ucSHaut)
      {
        ucGameAct = (ucGameAct> nbRomPerPage ? ucGameAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucGameAct == 0) romSelected = 0;
        if (romSelected > ucGameAct) romSelected = ucGameAct;
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;
    }
    else {
      ucSHaut = 0;
    }

    // -------------------------------------------------------------------------
    // They B key will exit out of the ROM selection without picking a new game
    // -------------------------------------------------------------------------
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }

    // -------------------------------------------------------------------
    // Any of these keys will pick the current ROM and try to load it...
    // -------------------------------------------------------------------
    if (keysCurrent() & KEY_A || keysCurrent() & KEY_Y || keysCurrent() & KEY_X)
    {
      if (gpFic[ucGameAct].uType != DIRECTORY)
      {
          bDone=true;
          ucGameChoice = ucGameAct;
          WAITVBL;
      }
      else
      {
        chdir(gpFic[ucGameAct].szName);
        MicroDSFindFiles();
        ucGameAct = 0;
        nbRomPerPage = (fileCount>=17 ? 17 : fileCount);
        uNbRSPage = (fileCount>=5 ? 5 : fileCount);
        if (ucGameAct>fileCount-nbRomPerPage) {
          firstRomDisplay=fileCount-nbRomPerPage;
          romSelected=ucGameAct-fileCount+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucGameAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }

    // --------------------------------------------
    // If the filename is too long... scroll it.
    // --------------------------------------------
    if (strlen(gpFic[ucGameAct].szName) > 30)
    {
      ucFlip++;
      if (ucFlip >= 25)
      {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+30)>strlen(gpFic[ucGameAct].szName))
        {
          ucFlop++;
          if (ucFlop >= 15)
          {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,gpFic[ucGameAct].szName+uLenFic,30);
        szName[30] = '\0';
        DSPrint(1,6+romSelected,2,szName);
      }
    }
    swiWaitForVBlank();
  }

  // Wait for some key to be pressed before returning
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);

  return 0x01;
}


// ---------------------------------------------------------------------------
// Write out the MicroDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings ~1000 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;

    if (bShow) DSPrint(6,23,0, (char*)"SAVING CONFIGURATION");

    // Set the global configuration version number...
    myGlobalConfig.config_ver = CONFIG_VERSION;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;

    // Find the slot we should save into...
    for (slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == myConfig.game_crc)  // Got a match?!
        {
            break;
        }
        if (AllConfigs[slot].game_crc == 0x00000000)  // Didn't find it... use a blank slot...
        {
            break;
        }
    }

    // --------------------------------------------------------------------------
    // Copy our current game configuration to the main configuration database...
    // --------------------------------------------------------------------------
    if (myConfig.game_crc != 0x00000000)
    {
        memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));
    }

    // Grab the directory we are currently in so we can restore it
    getcwd(myGlobalConfig.szLastPath, MAX_FILENAME_LEN);

    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // directory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }
    fp = fopen("/data/MicroDS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp); // Write the global config
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);         // Write the array of all configurations
        fclose(fp);
    } else DSPrint(4,23,0, (char*)"ERROR SAVING CONFIG FILE");

    if (bShow)
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,23,0, (char*)"                        ");
    }
}

void MapWAZS(void)
{
    myConfig.keymap[0]   = KBD_W;    // NDS D-Pad UP    mapped to W
    myConfig.keymap[1]   = KBD_A;    // NDS D-Pad LEFT  mapped to A
    myConfig.keymap[2]   = KBD_Z;    // NDS D-Pad DOWN  mapped to Z
    myConfig.keymap[3]   = KBD_S;    // NDS D-Pad RIGHT mapped to S
    myConfig.keymap[4]   = KBD_SPACE;// NDS A Button mapped to SPACE
    myConfig.keymap[5]   = KBD_ENTER;// NDS B Button mapped to ENTER
    myConfig.keymap[6]   = KBD_SPACE;// NDS X Button mapped to SPACE
    myConfig.keymap[7]   = KBD_ENTER;// NDS Y Button mapped to RETURN
    myConfig.keymap[8]   = KBD_CTRL; // NDS R Button mapped to CTRL
    myConfig.keymap[9]   = KBD_SHIFT;// NDS L Button mapped to SHIFT
}

void MapWASD(void)
{
    myConfig.keymap[0]   = KBD_W;    // NDS D-Pad UP    mapped to W
    myConfig.keymap[1]   = KBD_A;    // NDS D-Pad LEFT  mapped to A
    myConfig.keymap[2]   = KBD_S;    // NDS D-Pad DOWN  mapped to S
    myConfig.keymap[3]   = KBD_D;    // NDS D-Pad RIGHT mapped to D
    myConfig.keymap[4]   = KBD_SPACE;// NDS A Button mapped to SPACE
    myConfig.keymap[5]   = KBD_ENTER;// NDS B Button mapped to ENTER
    myConfig.keymap[6]   = KBD_SPACE;// NDS X Button mapped to SPACE
    myConfig.keymap[7]   = KBD_ENTER;// NDS Y Button mapped to RETURN
    myConfig.keymap[8]   = KBD_CTRL; // NDS R Button mapped to CTRL
    myConfig.keymap[9]   = KBD_SHIFT;// NDS L Button mapped to SHIFT
}

void MapIJKL(void)
{
    myConfig.keymap[0]   = KBD_I;    // NDS D-Pad UP    mapped to W
    myConfig.keymap[1]   = KBD_J;    // NDS D-Pad LEFT  mapped to A
    myConfig.keymap[2]   = KBD_K;    // NDS D-Pad DOWN  mapped to S
    myConfig.keymap[3]   = KBD_L;    // NDS D-Pad RIGHT mapped to D
    myConfig.keymap[4]   = KBD_SPACE;// NDS A Button mapped to SPACE
    myConfig.keymap[5]   = KBD_ENTER;// NDS B Button mapped to ENTER
    myConfig.keymap[6]   = KBD_SPACE;// NDS X Button mapped to SPACE
    myConfig.keymap[7]   = KBD_ENTER;// NDS Y Button mapped to RETURN
    myConfig.keymap[8]   = KBD_CTRL; // NDS R Button mapped to CTRL
    myConfig.keymap[9]   = KBD_SHIFT;// NDS L Button mapped to SHIFT
}

void SetDefaultGlobalConfig(void)
{
    // A few global defaults...
    memset(&myGlobalConfig, 0x00, sizeof(myGlobalConfig));
    myGlobalConfig.showFPS        = 0;    // Don't show FPS counter by default
    myGlobalConfig.debugger       = 0;    // Debugger is not shown by default
    myGlobalConfig.defMachine     = 0;    // Set to standard MC-10 by default
}

void SetDefaultGameConfig(void)
{
    myConfig.game_crc    = 0;    // No game in this slot yet

    MapWAZS();                   // Default to normal keyboard/cursor handling

    myConfig.machine     = myGlobalConfig.defMachine;   // Default is Tandy MC-10
    myConfig.dpad        = DPAD_NORMAL;                 // Normal DPAD use - mapped to cursor keys
    myConfig.autoLoad    = tape_guess_type();           // Default is to to auto-load games - try to autodetect
    myConfig.gameSpeed   = 0;                           // Default is 100% game speed
    myConfig.reserved0   = 0;
    myConfig.reserved1   = 0;
    myConfig.reserved2   = 0;
    myConfig.reserved3   = 0;
    myConfig.reserved4   = 0;
    myConfig.reserved5   = 0;
    myConfig.reserved6   = 0;
    myConfig.reserved7   = 0;
    myConfig.reserved8   = 0;
    myConfig.reserved9   = 0;
    myConfig.reserved10  = 0;
    myConfig.reserved11  = 0xA5;    // So it's easy to spot on an "upgrade" and we can re-default it

    for (int i=0; i<strlen(initial_file); i++)
    {
        initial_file[i] = toupper(initial_file[i]);
    }
}

// ----------------------------------------------------------
// Load configuration into memory where we can use it.
// The configuration is stored in MicroDS.DAT
// ----------------------------------------------------------
void LoadConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    if (ReadFileCarefully("/data/MicroDS.DAT", (u8*)&myGlobalConfig, sizeof(myGlobalConfig), 0))  // Read Global Config
    {
        ReadFileCarefully("/data/MicroDS.DAT", (u8*)&AllConfigs, sizeof(AllConfigs), sizeof(myGlobalConfig)); // Read the full game array of configs

        if (myGlobalConfig.config_ver != CONFIG_VERSION)
        {
            memset(&AllConfigs, 0x00, sizeof(AllConfigs));
            SetDefaultGameConfig();
            SetDefaultGlobalConfig();
            SaveConfig(FALSE);
        }
    }
    else    // Not found... init the entire database...
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SetDefaultGlobalConfig();
        SaveConfig(FALSE);
    }}

// -------------------------------------------------------------------------
// Try to match our loaded game to a configuration my matching CRCs
// -------------------------------------------------------------------------
void FindConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();

    for (u16 slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
        {
            memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
            break;
        }
    }
}


// ------------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off
// their option choices for the currently running game into the MicroDS.DAT
// configuration database. When games are loaded back up, MicroDS.DAT is read
// to see if we have a match and the user settings can be restored for the game.
// ------------------------------------------------------------------------------
struct options_t
{
    const char  *label;
    const char  *option[12];
    u8          *option_val;
    u8           option_max;
};

const struct options_t Option_Table[2][20] =
{
    // Game Specific Configuration
    {
        {"MACHINE TYPE",   {"MC10 (20K RAM)", "MC10 (32K RAM)", "MCX-128"},            &myConfig.machine,           3},
        {"AUTO LOAD",      {"NO", "CLOAD [RUN]", "CLOADM [EXEC]"},                     &myConfig.autoLoad,          3},
        {"GAME SPEED",     {"100%", "110%", "120%", "130%", "90%", "80%"},             &myConfig.gameSpeed,         6},
        {"NDS D-PAD",      {"NORMAL", "SLIDE-N-GLIDE", "DIAGONALS"},                   &myConfig.dpad,              3},
        {NULL,             {"",      ""},                                              NULL,                        1},
    },
    // Global Options
    {
        {"MACHINE TYPE",   {"MC10 (20K RAM)", "MC10 (32K RAM)", "MCX-128"},            &myGlobalConfig.defMachine,  3},
        {"FPS",            {"OFF", "ON", "ON FULLSPEED"},                              &myGlobalConfig.showFPS,     3},
        {"DEBUGGER",       {"OFF", "ON"},                                              &myGlobalConfig.debugger,    2},
        {NULL,             {"",      ""},                                              NULL,                        1},
    }
};


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    s16 len=0;

    DSPrint(1,21, 0, (char *)"                              ");
    if (bFullDisplay)
    {
        while (true)
        {
            sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            DSPrint(1,6+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<15; i++)
        {
            DSPrint(1,6+i, 0, (char *)"                               ");
        }
    }

    DSPrint(1,22, 0, (char *)" B=EXIT, X=GLOBAL, START=SAVE  ");
    return len;
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void MicroDSGameOptions(bool bIsGlobal)
{
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;

    option_table = (bIsGlobal ? 1:0);

    idx=display_options_list(true);
    optionHighlighted = 0;
    while (keysCurrent() != 0)
    {
        WAITVBL;
    }
    while (!bDone)
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[option_table][optionHighlighted].option_val)) == 0)
                    *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max -1;
                else
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) - 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,6+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if (keysCurrent() & (KEY_X)) // Toggle Table
            {
                option_table ^= 1;
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                option_table = 0;   // Reset for next time
                break;
            }
        }
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }

    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
char szCha[34];
void DisplayKeymapName(u32 uY)
{
  sprintf(szCha," PAD UP    : %-17s",szKeyName[myConfig.keymap[0]]);
  DSPrint(1, 6,(uY== 6 ? 2 : 0),szCha);
  sprintf(szCha," PAD LEFT  : %-17s",szKeyName[myConfig.keymap[1]]);
  DSPrint(1, 7,(uY== 7 ? 2 : 0),szCha);
  sprintf(szCha," PAD DOWN  : %-17s",szKeyName[myConfig.keymap[2]]);
  DSPrint(1, 8,(uY== 8 ? 2 : 0),szCha);
  sprintf(szCha," PAD RIGHT : %-17s",szKeyName[myConfig.keymap[3]]);
  DSPrint(1, 9,(uY== 9 ? 2 : 0),szCha);
  sprintf(szCha," KEY A     : %-17s",szKeyName[myConfig.keymap[4]]);
  DSPrint(1,10,(uY== 10 ? 2 : 0),szCha);
  sprintf(szCha," KEY B     : %-17s",szKeyName[myConfig.keymap[5]]);
  DSPrint(1,11,(uY== 11 ? 2 : 0),szCha);
  sprintf(szCha," KEY X     : %-17s",szKeyName[myConfig.keymap[6]]);
  DSPrint(1,12,(uY== 12 ? 2 : 0),szCha);
  sprintf(szCha," KEY Y     : %-17s",szKeyName[myConfig.keymap[7]]);
  DSPrint(1,13,(uY== 13 ? 2 : 0),szCha);
  sprintf(szCha," KEY R     : %-17s",szKeyName[myConfig.keymap[8]]);
  DSPrint(1,14,(uY== 14 ? 2 : 0),szCha);
  sprintf(szCha," KEY L     : %-17s",szKeyName[myConfig.keymap[9]]);
  DSPrint(1,15,(uY== 15 ? 2 : 0),szCha);
}

u8 keyMapType = 0;
void SwapKeymap(void)
{
    keyMapType = (keyMapType+1) % 3;
    switch (keyMapType)
    {
        case 0: MapWAZS();     DSPrint(12,23,0,("CURSORS")); break;
        case 1: MapWASD();     DSPrint(12,23,0,(" WASD  ")); break;
        case 2: MapIJKL();     DSPrint(12,23,0,(" IJKL  ")); break;
    }
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(12,23,0,("         "));
}


// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void MicroDSChangeKeymap(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY= 6, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);

  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  DSPrint(1 ,19,0,("   D-PAD : CHANGE KEY MAP    "));
  DSPrint(1 ,20,0,("       B : RETURN MAIN MENU  "));
  DSPrint(1 ,21,0,("       X : SWAP KEYMAP TYPE  "));
  DSPrint(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);

  bIndTch = myConfig.keymap[0];

  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_X | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;

  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 6 ? 15 : ucY -1);
        bIndTch = myConfig.keymap[ucY-6];
        ucHaut=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        DisplayKeymapName(32);
        ucY = (ucY == 15 ? 6 : ucY +1);
        bIndTch = myConfig.keymap[ucY-6];
        ucBas=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }

    if (keysCurrent() & KEY_START)
    {
        SaveConfig(true); // Save options
    }

    if (keysCurrent() & KEY_B)
    {
      bOK = 1;  // Exit menu
    }

    if (keysCurrent() & KEY_LEFT)
    {
        if (ucL == 0) {
          bIndTch = (bIndTch == 0 ? (MAX_KEY_OPTIONS-1) : bIndTch-1);
          ucL=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 7) ucL = 0;
        }
    }
    else
    {
        ucL = 0;
    }

    if (keysCurrent() & KEY_RIGHT)
    {
        if (ucR == 0)
        {
          bIndTch = (bIndTch == (MAX_KEY_OPTIONS-1) ? 0 : bIndTch+1);
          ucR=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else
        {
          ucR++;
          if (ucR > 7) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }

    if (keysCurrent() & KEY_X)
    {
        SwapKeymap();
        bIndTch = myConfig.keymap[ucY-6];
        DisplayKeymapName(ucY);
        while (keysCurrent() & KEY_X)
            ;
        WAITVBL
    }

    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// -----------------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename, size and CRC32
// -----------------------------------------------------------------------------------------
void DisplayFileName(void)
{
    sprintf(szName, "[%d K] [CRC: %08X]", file_size/1024, file_crc);
    DSPrint((16 - (strlen(szName)/2)),19,0,szName);

    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    DSPrint((16 - (strlen(szName)/2)),21,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 35)   // If there is more than a few characters left, show it on the 2nd line
    {
        if (strlen(gpFic[ucGameChoice].szName) <= 60)
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+30);
        }
        else
        {
            sprintf(szName,"%s",gpFic[ucGameChoice].szName+strlen(gpFic[ucGameChoice].szName)-30);
        }

        if (strlen(szName)>30) szName[30]='\0';
        DSPrint((16 - (strlen(szName)/2)),22,0,szName);
    }
}

//*****************************************************************************
// Display info screen and change options "main menu"
//*****************************************************************************
void dispInfoOptions(u32 uY)
{
    DSPrint(2, 7,(uY== 7 ? 2 : 0),("         LOAD  GAME         "));
    DSPrint(2, 9,(uY== 9 ? 2 : 0),("         PLAY  GAME         "));
    DSPrint(2,11,(uY==11 ? 2 : 0),("       DEFINE  KEYS         "));
    DSPrint(2,13,(uY==13 ? 2 : 0),("         GAME  OPTIONS      "));
    DSPrint(2,15,(uY==15 ? 2 : 0),("       GLOBAL  OPTIONS      "));
    DSPrint(2,17,(uY==17 ? 2 : 0),("         QUIT  EMULATOR     "));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    DSPrint(5,10,0,("   NO GAME SELECTED   "));
    DSPrint(5,12,0,("  PLEASE, USE OPTION  "));
    DSPrint(5,14,0,("      LOAD  GAME      "));
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    dispInfoOptions(ucY);
}


void ReadFileCRCAndConfig(void)
{
    keyMapType = 0;

    // ----------------------------------------------------------------------------------
    // Clear the entire ROM buffer[] - fill with 0xFF to emulate non-responsive memory
    // ----------------------------------------------------------------------------------
    memset(TapeBuffer, 0xFF, MAX_FILE_SIZE);

    // Determine the file type based on the filename extension
    if (strstr(gpFic[ucGameChoice].szName, ".c10") != 0) micro_mode = MODE_CAS;
    if (strstr(gpFic[ucGameChoice].szName, ".C10") != 0) micro_mode = MODE_CAS;

    // Save the initial filename and file - we need it for save/restore of state
    strcpy(initial_file, gpFic[ucGameChoice].szName);
    strcpy(last_file, gpFic[ucGameChoice].szName);
    getcwd(initial_path, MAX_FILENAME_LEN);
    getcwd(last_path, MAX_FILENAME_LEN);

    // Grab the all-important file CRC - this also loads the file into TapeBuffer[]
    getfile_crc(gpFic[ucGameChoice].szName);

    loadgame(gpFic[ucGameChoice].szName);

    FindConfig();    // Try to find keymap and config for this file...
}


// ----------------------------------------------------------------------
// Read file twice and ensure we get the same CRC... if not, do it again
// until we get a clean read. Return the filesize to the caller...
// ----------------------------------------------------------------------
u32 ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset)
{
    u32 crc1 = 0;
    u32 crc2 = 1;
    u32 fileSize = 0;

    // --------------------------------------------------------------------------------------------
    // I've seen some rare issues with reading files from the SD card on a DSi so we're doing
    // this slow and careful - we will read twice and ensure that we get the same CRC both times.
    // --------------------------------------------------------------------------------------------
    do
    {
        // Read #1
        crc1 = 0xFFFFFFFF;
        FILE* file = fopen(filename, "rb");
        if (file)
        {
            if (buf_offset) fseek(file, buf_offset, SEEK_SET);
            fileSize = fread(buf, 1, buf_size, file);
            crc1 = getCRC32(buf, buf_size);
            fclose(file);
        }

        // Read #2
        crc2 = 0xFFFFFFFF;
        FILE* file2 = fopen(filename, "rb");
        if (file2)
        {
            if (buf_offset) fseek(file2, buf_offset, SEEK_SET);
            fread(buf, 1, buf_size, file2);
            crc2 = getCRC32(buf, buf_size);
            fclose(file2);
        }
   } while (crc1 != crc2); // If the file couldn't be read, file_size will be 0 and the CRCs will both be 0xFFFFFFFF

   return fileSize;
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void MicroDSChangeOptions(void)
{
  u16 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 7, bOK=0;

  // Upper Screen Background
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(splashTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(splashMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) splashPal,(void*) BG_PALETTE,256*2);

  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);

  // Lower Screen Background
  BottomScreenOptions();

  dispInfoOptions(ucY);

  if (ucGameChoice != -1)
  {
      DisplayFileName();
  }

  while (!bOK) {
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        dispInfoOptions(32);
        ucY = (ucY == 7 ? 17 : ucY -2);
        ucHaut=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent()  & KEY_DOWN) {
      if (!ucBas) {
        dispInfoOptions(32);
        ucY = (ucY == 17 ? 7 : ucY +2);
        ucBas=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
    }
    else {
      ucBas = 0;
    }
    if (keysCurrent()  & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        switch (ucY) {
          case 7 :      // LOAD GAME
            MicroDSLoadFile();
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            BottomScreenOptions();
            if (ucGameChoice != -1)
            {
                ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
                DisplayFileName();    // And put up the filename on the bottom screen
            }
            ucY = 9;
            dispInfoOptions(ucY);
            break;
          case 9 :     // PLAY GAME
            if (ucGameChoice != -1)
            {
                bOK = 1;
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 11 :     // REDEFINE KEYS
            if (1==1)
            {
                MicroDSChangeKeymap();
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
                NoGameSelected(ucY);
            }
            break;
          case 13 :     // GAME OPTIONS
            if (1==1)
            {
                MicroDSGameOptions(false);
                BottomScreenOptions();
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else
            {
               NoGameSelected(ucY);
            }
            break;

          case 15 :     // GLOBAL OPTIONS
            MicroDSGameOptions(true);
            BottomScreenOptions();
            dispInfoOptions(ucY);
            DisplayFileName();
            break;

          case 17 :     // QUIT EMULATOR
            exit(1);
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1)
      {
        bOK = 1;
      }
      else
      {
        NoGameSelected(ucY);
      }
    }
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************
ITCM_CODE void DSPrint(int iX,int iY,int iScr,char *szMessage)
{
  u16 *pusScreen,*pusMap;
  u16 usCharac;
  char *pTrTxt=szMessage;

  pusScreen=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );

  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt++;
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // Faster than strcpy/strtoupper

    if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);                   // Will render as a vertical bar
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');          // Number from 0-9 or punctuation
    else
      usCharac=*(pusMap+32+(ch)-'@');       // Character from A-Z
    *pusScreen++=usCharac;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait)
{
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out to black
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}


/*********************************************************************************
 * Keyboard Key Buffering Engine...
 ********************************************************************************/
u8 BufferedKeys[32];
u8 BufferedKeysWriteIdx=0;
u8 BufferedKeysReadIdx=0;
void BufferKey(u8 key)
{
    BufferedKeys[BufferedKeysWriteIdx] = key;
    BufferedKeysWriteIdx = (BufferedKeysWriteIdx+1) % 32;
}

// ---------------------------------------------------------------------------------------
// Called every frame... so 1/50th or 1/60th of a second. We will virtually 'press' and
// hold the key for roughly a tenth of a second and be smart about shift keys...
// ---------------------------------------------------------------------------------------
void ProcessBufferedKeys(void)
{
    static u8 next_dampen_time = 8;
    static u8 dampen = 0;
    static u8 buf_held = 0;

    if (++dampen >= next_dampen_time) // Roughly 150ms... experimentally good enough for the MC-10
    {
        kbd_keys_pressed = 0;
        if (dampen == next_dampen_time)
        {
            buf_held = 0x00;
        }
        else
        {
            if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
            {
                buf_held = BufferedKeys[BufferedKeysReadIdx];
                BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
                next_dampen_time = 8;
                if (buf_held == 255) {buf_held = 0; kbd_key = 0;}
            }
            else
            {
                buf_held = 0x00;
            }
            dampen = 0;
        }
    }

    // See if the shift key should be virtually pressed along with this buffered key...
    if (buf_held) {kbd_key = buf_held; kbd_keys[kbd_keys_pressed++] = buf_held;}
}


/*********************************************************************************
 * Init MC-10 Emulation Engine for that game
 ********************************************************************************/
u8 MC10Init(char *szGame)
{
  u8 RetFct,uBcl;
  u16 uVide;

  // We've got some debug data we can use for development... reset these.
  memset(debug, 0x00, sizeof(debug));
  DX = DY = 0;

  // -----------------------------------------------------------------
  // Change graphic mode to initiate emulation.
  // Here we can claim back 128K of VRAM which is otherwise unused
  // but we can use it for fast memory swaps and look-up-tables.
  // -----------------------------------------------------------------
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);      // This is our top emulation screen (where the game is played)
  vramSetBankB(VRAM_B_LCD);

  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8);
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;

  // Init the page flipping buffer...
  for (uBcl=0;uBcl<192;uBcl++)
  {
     uVide=(uBcl/12);
     dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
  }

  RetFct = loadgame(szGame);      // Load up the .c10 game

  ResetMicroComputer();

  // Return with result
  return (RetFct);
}

/*********************************************************************************
 * Run the emul
 ********************************************************************************/
void RunMicroComputer(void)
{
  micro_reset();                        // Ensure the MC-10 Emulation is ready
  BottomScreenKeyboard();               // Show the game-related screen with keypad / keyboard
}

// ------------------------------------------------------------------------------------
// These colors were derived by using other emulators and taking screenshots and then
// using GIMPs color-picker to try and get as close as possible. At first I was just
// assigning RGB values that made sense - for example FB_CYAN was 0x00FFFF but in
// reality the color names are only approximations of the actual colors rendered by
// the Motorola video chip... these aren't perfect but they will be good enough.
// ------------------------------------------------------------------------------------
u8 MC10_palette[16*3] =
{
  0x00, 0x00, 0x00, // FB_BLACK

  0x00, 0xFF, 0x00, // FB_GREEN
  0xFF, 0xFF, 0x83, // FB_YELLOW
  0x1B, 0x16, 0xEB, // FB_BLUE
  0xC0, 0x0E, 0x24, // FB_RED

  0xF0, 0xF0, 0xF0, // FB_BUFF (White-ish)
  0x1D, 0x9C, 0x5D, // FB_CYAN (slightly more greenish)
  0xFD, 0x25, 0xFF, // FB_MAGENTA (slightly more purplish)
  0xFE, 0x42, 0x0D, // FB_ORANGE (slightly more reddish)

  0x00, 0x80, 0xFF, // Artifact BLUE
  0xFF, 0x80, 0x00, // Artifact ORANGE
  0x00, 0x80, 0x00, // Artifact Green

  0x10, 0x60, 0x10, // Dark  Green Text
  0x78, 0x50, 0x20, // Dark  Orange Text
  0x28, 0xE0, 0x28, // Light Green Text
  0xF0, 0xB0, 0x40, // Light Orange Text
};


/**********************************************************************************
 * Set the MC-10 color palette - the standard 9 colors plus some extras we use
 * for artifacting and for text modes...
 *********************************************************************************/
void MC10SetPalette(void)
{
  u8 uBcl,r,g,b;

  for (uBcl=0;uBcl<16;uBcl++)
  {
    r = (u8) ((float) MC10_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) MC10_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) MC10_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *filename)
{
    DSPrint(11,13,6, "LOADING...");
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;

    file_crc = getFileCrc(filename);        // The CRC is used as a unique ID to save out High Scores and Configuration...

    DSPrint(11,13,6, "          ");
}


/** loadgame() ******************************************************************/
/* Open a rom file from file system and load it into the TapeBuffer[] buffer    */
/********************************************************************************/
u8 loadgame(const char *filename)
{
  u8 bOK = 0;
  int romSize = 0;

  FILE* handle = fopen(filename, "rb");
  if (handle != NULL)
  {
    // Get file size the 'fast' way - use fstat() instead of fseek() or ftell()
    struct stat stbuf;
    (void)fstat(fileno(handle), &stbuf);
    romSize = stbuf.st_size;
    fclose(handle); // We only need to close the file - the game ROM is now sitting in TapeBuffer[] from the getFileCrc() handler

    last_file_size = (u32)romSize;
  }

  return bOK;
}

void vblankIntro()
{
  vusCptVBL++;
}

// --------------------------------------------------------------
// Intro with portabledev logo and new PHEONIX-EDITION version
// --------------------------------------------------------------
void intro_logo(void)
{
  bool bOK;

  // Init graphics
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  irqSet(IRQ_VBLANK, vblankIntro);
  irqEnable(IRQ_VBLANK);

  // Init BG
  int bg1 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  // Init sub BG
  int bg1s = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

  REG_BLDCNT = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY = 16;
  REG_BLDCNT_SUB = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY_SUB = 16;

  mmEffect(SFX_MUS_INTRO);

  // Show splash
  decompress(splashTiles, bgGetGfxPtr(bg1), LZ77Vram);
  decompress(splashMap, (void*) bgGetMapPtr(bg1), LZ77Vram);
  dmaCopy((void *) splashPal,(u16*) BG_PALETTE,256*2);

  decompress(splash_botTiles, bgGetGfxPtr(bg1s), LZ77Vram);
  decompress(splash_botMap, (void*) bgGetMapPtr(bg1s), LZ77Vram);
  dmaCopy((void *) splash_botPal,(u16*) BG_PALETTE_SUB,256*2);

  FadeToColor(0,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,0,3);

  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; } // 0x1FFF = key or touch screen
  vusCptVBL=0;bOK=false;
  while (!bOK && (vusCptVBL<3*60)) { if (keysCurrent() & 0x1FFF ) bOK=true; }
  bOK=false;
  while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; }

  FadeToColor(1,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,16,3);
}


void _putchar(char character) {}

// End of file
