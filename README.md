![image](./png/micro-ds.png)

# MicroDS
A Tandy MC-10 Micro Color Computer emulator for the DS/DSi/XL/LL

Copyright (c) 2025 by Dave Bernazzani (wavemotion-dave)

![image](./png/splash.png)

Features :
-----------------------
* Tandy MicroColor Computer MC-10 emulation with solid MC6803 CPU core.
* 4K Internal RAM + 16K External Expansion RAM for 20K total (+128 byte CPU RAM).
* Optional memory expansion to 32K in machine configuration.
* Cassette (.c10) support for loading games and programs.
* Save/Load Game State (one slot).
* LCD Screen Swap (press and hold L+R+X during gameplay).
* LCD Screen snapshot - (press and hold L+R+Y during gameplay and the .png file will be written to the SD card).
* Virtual keyboard stylized to the MC-10 with the ability to map any keyboard key to DS buttons.
* Full speed, full sound and full frame-rate even on older hardware.

![image](./png/kbd.png)

Copyright :  
-----------------------
MicroDS is Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)

This is a derivitive work of Dragon 32 Emu Copyright (c) 2018 Eyal Abraham (eyalabraham)
which can be found on github:

https://github.com/eyalabraham/dragon32-emu

The dragon32-emu graciously allows modification and use via the lenient MIT Licence.

As far as I'm concerned: as long as there is no commercial use (i.e. no profit is made),
copying and distribution of this emulator, it's source code and associated readme files,
with or without modification, are permitted in any medium without royalty provided this 
copyright notice is used and wavemotion-dave and eyalabraham are thanked profusely.

Royalty Free Music for the opening jingle provided by Anvish Parker

lzav compression (for save states) is Copyright (c) 2023-2025 Aleksey 
Vaneev and used by permission of the generous MIT license.

The MicroDS emulator is offered as-is, without any warranty.

BIOS/BASIC Files Needed :
-----------------------

You will need the MICROBASIC ROM which must be named either MC10.BIN or MC10.ROM (same file, just different naming conventions) and it 
must be placed in the same directory as the emulator or else /roms/bios

Loading Games :
-----------------------
This MC-10 emulator supports .C10 cassette files. These are the most popular format to find on the web for the MC-10 machine.

Games/Prorams come in two main varieties: BASIC and Machine Language. Each requires a different LOAD command in MICROBASIC. 
The emulator tries to take a 'best guess' as to what kind of program is being loaded and is generally 95% accurate... but you 
can override this on a per-game configuration basis.

Loading a machine code language program vs a BASIC program:

```
CLOADM
EXEC

CLOAD
RUN
```

You can press the START button to automatically issue the load command once you've gotten to the main MICROBASIC screen:

![image](./png/emuscreen.png)


Configuration Options :
-----------------------
MicroDS includes global options (applied to the emulator as a whole and all games) and game-specific options (applied to just the one game file that was loaded).

Key Mapping Options :
-----------------------
Each game can individually configure any of the 12 DS buttons (D-PAD, ABXY, L/R, START/SELECT) to a single CoCo/Dragon joystick or keyboard button. The default is for the D-PAD 
and Button A to replicate the joystick but you can configure as you like. Pressing the X button on this screen will toggle between some preset defaults for common joystick 
maps - such as using the D-PAD as the keyboard cursor keys (up, down, left, right arrows).

Keyboards :
-----------------------
The emulator supports a virtual touch-screen version of the MC-10 keyboard. Note that due to the limitations of the DS touch-screen where only one key can be
pressed at a time, the SHIFT key works like a temporary toggle. Press it and then the next key you press will be SHIFT-ed.

![image](./png/kbd.png)

Screenshot and Screen Swap:
-----------------------
Pressing and holding the L/R shoulder buttons plus X will exchange the top and bottom screens. However, only the bottom screen is touch-sensitive so you would still need to press on the bottom screen to make the touch screen work.

Pressing and holding the L/R shoulder buttons plus Y will create a screen snapshot of the game screen. It will be time/date stamped and written to the SD card in the same directory as the game file.

Known Issues:
-----------------------
* None

Compile Instructions :
-----------------------
gcc (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0
libnds 1.8.2-1
I use Ubuntu and the Pacman repositories (devkitpro-pacman version 6.0.1-7).  I'm told it should also build under 
Windows but I've never done it and don't know how.

If you've got the nds libraries above setup correctly it should be a matter of typing:
* _make clean_
* _make_

To build the emulator. The output of this is ColecoDS.nds with a version as set in the MAKEFILE.
I use the following standard environment variables that are SET on Ubuntu:
* DEVKITARM=/opt/devkitpro/devkitARM
* DEVKITPPC=/opt/devkitpro/devkitPPC
* DEVKITPRO=/opt/devkitpro

To create the soundbank.bin and soundbank.h (sound effects) file in the data directory:

mmutil -osoundbank.bin -hsoundbank.h -d *.wav

And then move the soundbank.h file to the arm9/sources directory

Versions :
-----------------------
0.7: First public beta.
