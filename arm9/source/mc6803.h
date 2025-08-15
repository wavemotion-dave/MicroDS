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


/*
 * mc6803.h
 *
 */

#ifndef __MC6803_H__
#define __MC6803_H__
#include    <nds.h>
#include    <stdint.h>

#define     ADDR_DIRECT             1           // Direct
#define     ADDR_INHERENT           2           // Inherent
#define     ADDR_RELATIVE           3           // 8-bit address offset
#define     ADDR_INDEXED            4           // Indexed
#define     ADDR_EXTENDED           5           // Extended
#define     ADDR_IMMEDIATE          6           // 8-bit immediate
#define     ADDR_LIMMEDIATE         7           // 16-bit immediate
#define     ILLEGAL_OP              8

char *op_name[256] =
{
    "ILL",
    "NOP",
    "ILL",
    "ILL",
    "LSRD",
    "LSLD",
    "TAP",
    "TPA",
    "INX",
    "DEX",
    "CLV",
    "SEV",
    "CLC",
    "SEC",
    "CLI",
    "SEI",
    
    "SBA",
    "CBA",
    "ILL",
    "ILL",
    "ILL",
    "ILL",
    "TAB",
    "TBA",
    "ILL",
    "DAA",
    "ILL",
    "ABA",
    "ILL",
    "ILL",
    "ILL",
    "ILL",
    
    "BRA",
    "BRN",
    "BHI",
    "BLS",
    "BCC",
    "BCS",
    "BNE",
    "BEQ",
    "BVC",
    "BVS",
    "BPL",
    "BMI",
    "BGE",
    "BLT",
    "BGT",
    "BLE",
    
    "TSX",
    "INS",
    "PULA",
    "PULB",
    "DES",
    "TXS",
    "PSHA",
    "PSHB",
    "PULX",
    "RTS",
    "ABX",
    "RTI",
    "PSHX",
    "MUL",
    "WAI",
    "SWI",
    
    "NEGA",
    "ILL",
    "ILL",
    "COMA",
    "LSRA",
    "ILL",
    "RORA",
    "ASRA",
    "LSLA",
    "ROLA",
    "DECA",
    "ILL",
    "INCA",
    "TSTA",
    "ILL",
    "CLRA",

    "NEGB",
    "ILL",
    "ILL",
    "COMB",
    "LSRB",
    "ILL",
    "RORB",
    "ASRB",
    "LSLB",
    "ROLB",
    "DECB",
    "ILL",
    "INCB",
    "TSTB",
    "ILL",
    "CLRB",

    "NEG",
    "ILL",
    "ILL",
    "COM",
    "LSR",
    "LSL",
    "ROR",
    "ASR",
    "ILL",
    "ROL",
    "DEC",
    "ILL",
    "INC",
    "TST",
    "JMP",
    "CLR",

    "NEG",
    "ILL",
    "ILL",
    "COM",
    "LSR",
    "ILL",
    "ROR",
    "ASR",
    "LSL",
    "ROL",
    "DEC",
    "ILL",
    "INC",
    "TST",
    "JMP",
    "CLR",

    "SUBA",
    "CMPA",
    "SBCA",
    "SUBD",
    "ANDA",
    "BITA",
    "LDA",
    "ILL",
    "EORA",
    "ADCA",
    "ORA",
    "ADDA",
    "CMPX",
    "BSR",
    "LDS",
    "ILL",

    "SUBA",
    "CMPA",
    "SBCA",
    "SUBD",
    "ANDA",
    "BITA",
    "LDA",
    "STA",
    "EORA",
    "ADCA",
    "ORA",
    "ADDA",
    "CMPX",
    "JSR",
    "LDS",
    "STS",

    "SUBA",
    "CMPA",
    "SBCA",
    "SUBD",
    "ANDA",
    "BITA",
    "LDA",
    "STA",
    "EORA",
    "ADCA",
    "ORA",
    "ADDA",
    "CMPX",
    "JSR",
    "LDS",
    "STS",

    "SUBA",
    "CMPA",
    "SBCA",
    "SUBD",
    "ANDA",
    "BITA",
    "LDA",
    "STA",
    "EORA",
    "ADCA",
    "ORA",
    "ADDA",
    "CMPX",
    "JSR",
    "LDS",
    "STS",

    "SUBB",
    "CMPB",
    "SBCB",
    "ADDD",
    "ADDB",
    "BITB",
    "LDB",
    "ILL",
    "EORB",
    "ADCB",
    "ORB",
    "ADDB",
    "LDD",
    "ILL",
    "LDX",
    "ILL",

    "SUBB",
    "CMPB",
    "SBCB",
    "ADDD",
    "ADDB",
    "BITB",
    "LDB",
    "STB",
    "EORB",
    "ADCB",
    "ORB",
    "ADDB",
    "LDD",
    "STD",
    "LDX",
    "STX",

    "SUBB",
    "CMPB",
    "SBCB",
    "ADDD",
    "ADDB",
    "BITB",
    "LDB",
    "STB",
    "EORB",
    "ADCB",
    "ORB",
    "ADDB",
    "LDD",
    "STD",
    "LDX",
    "STX",

    "SUBB",
    "CMPB",
    "SBCB",
    "ADDD",
    "ADDB",
    "BITB",
    "LDB",
    "STB",
    "EORB",
    "ADCB",
    "ORB",
    "ADDB",
    "LDD",
    "STD",
    "LDX",
    "STX"
};

typedef struct
{
    uint8_t  op;
    uint8_t  mode;
    uint8_t  cycles;
    uint8_t  bytes;
} machine_code_t;

machine_code_t machine_code[256] __attribute__((section(".dtcm"))) =
{
    {0x00, ILLEGAL_OP     , 0 , 1},
    {0x01, ADDR_INHERENT  , 2 , 1}, // NOP
    {0x02, ILLEGAL_OP     , 0 , 1},
    {0x03, ILLEGAL_OP     , 0 , 1},
    {0x04, ADDR_INHERENT  , 3 , 1}, // LSRD
    {0x05, ADDR_INHERENT  , 3 , 1}, // LSLD
    {0x06, ADDR_INHERENT  , 2 , 1}, // TAP
    {0x07, ADDR_INHERENT  , 2 , 1}, // TPA
    {0x08, ADDR_INHERENT  , 3 , 1}, // INX
    {0x09, ADDR_INHERENT  , 3 , 1}, // DEX
    {0x0a, ADDR_INHERENT  , 2 , 1}, // CLV
    {0x0b, ADDR_INHERENT  , 2 , 1}, // SEV
    {0x0c, ADDR_INHERENT  , 2 , 1}, // CLC
    {0x0d, ADDR_INHERENT  , 2 , 1}, // SEC
    {0x0e, ADDR_INHERENT  , 2 , 1}, // CLI
    {0x0f, ADDR_INHERENT  , 2 , 1}, // SEI

    {0x10, ADDR_INHERENT  , 2 , 1}, // SBA
    {0x11, ADDR_INHERENT  , 2 , 1}, // CBA
    {0x12, ILLEGAL_OP     , 0 , 1},
    {0x13, ILLEGAL_OP     , 0 , 1},
    {0x14, ILLEGAL_OP     , 0 , 1},
    {0x15, ILLEGAL_OP     , 0 , 1},
    {0x16, ADDR_INHERENT  , 2 , 1}, // TAB
    {0x17, ADDR_INHERENT  , 2 , 1}, // TBA
    {0x18, ILLEGAL_OP     , 0 , 1},
    {0x19, ADDR_INHERENT  , 2 , 1}, // DAA
    {0x1a, ILLEGAL_OP     , 0 , 1},
    {0x1b, ADDR_INHERENT  , 2 , 1}, // ABA
    {0x1c, ILLEGAL_OP     , 0 , 1},
    {0x1d, ILLEGAL_OP     , 0 , 1},
    {0x1e, ILLEGAL_OP     , 0 , 1},
    {0x1f, ILLEGAL_OP     , 0 , 1},

    {0x20, ADDR_RELATIVE  , 3 , 2}, // BRA
    {0x21, ADDR_RELATIVE  , 3 , 2}, // BRN
    {0x22, ADDR_RELATIVE  , 3 , 2}, // BHI
    {0x23, ADDR_RELATIVE  , 3 , 2}, // BLS
    {0x24, ADDR_RELATIVE  , 3 , 2}, // BCC
    {0x25, ADDR_RELATIVE  , 3 , 2}, // BCS
    {0x26, ADDR_RELATIVE  , 3 , 2}, // BNE
    {0x27, ADDR_RELATIVE  , 3 , 2}, // BEQ
    {0x28, ADDR_RELATIVE  , 3 , 2}, // BVC
    {0x29, ADDR_RELATIVE  , 3 , 2}, // BVS
    {0x2a, ADDR_RELATIVE  , 3 , 2}, // BPL
    {0x2b, ADDR_RELATIVE  , 3 , 2}, // BMI
    {0x2c, ADDR_RELATIVE  , 3 , 2}, // BGE
    {0x2d, ADDR_RELATIVE  , 3 , 2}, // BLT
    {0x2e, ADDR_RELATIVE  , 3 , 2}, // BGT
    {0x2f, ADDR_RELATIVE  , 3 , 2}, // BLE

    {0x30, ADDR_INHERENT  , 3 , 1}, // TSX
    {0x31, ADDR_INHERENT  , 3 , 1}, // INS
    {0x32, ADDR_INHERENT  , 4 , 1}, // PULA
    {0x33, ADDR_INHERENT  , 4 , 1}, // PULB
    {0x34, ADDR_INHERENT  , 3 , 1}, // DES
    {0x35, ADDR_INHERENT  , 3 , 1}, // TXS
    {0x36, ADDR_INHERENT  , 3 , 1}, // PSHA
    {0x37, ADDR_INHERENT  , 3 , 1}, // PSHB
    {0x38, ADDR_INHERENT  , 5 , 1}, // PULX
    {0x39, ADDR_INHERENT  , 5 , 1}, // RTS
    {0x3a, ADDR_INHERENT  , 3 , 1}, // ABX
    {0x3b, ADDR_INHERENT  ,10 , 1}, // RTI
    {0x3c, ADDR_INHERENT  , 4 , 1}, // PSHX
    {0x3d, ADDR_INHERENT  ,10 , 1}, // MUL
    {0x3e, ADDR_INHERENT  , 9 , 1}, // WAI
    {0x3f, ADDR_INHERENT  ,12 , 1}, // SWI

    {0x40, ADDR_INHERENT  , 2 , 1}, // NEGA
    {0x41, ILLEGAL_OP     , 0 , 1},
    {0x42, ILLEGAL_OP     , 0 , 1},
    {0x43, ADDR_INHERENT  , 2 , 1}, // COMA
    {0x44, ADDR_INHERENT  , 2 , 1}, // LSRA
    {0x45, ILLEGAL_OP     , 0 , 1},
    {0x46, ADDR_INHERENT  , 2 , 1}, // RORA
    {0x47, ADDR_INHERENT  , 2 , 1}, // ASRA
    {0x48, ADDR_INHERENT  , 2 , 1}, // LSLA
    {0x49, ADDR_INHERENT  , 2 , 1}, // ROLA
    {0x4a, ADDR_INHERENT  , 2 , 1}, // DECA
    {0x4b, ILLEGAL_OP     , 0 , 1},
    {0x4c, ADDR_INHERENT  , 2 , 1}, // INCA
    {0x4d, ADDR_INHERENT  , 2 , 1}, // TSTA
    {0x4e, ILLEGAL_OP     , 0 , 1},
    {0x4f, ADDR_INHERENT  , 2 , 1}, // CLRA

    {0x50, ADDR_INHERENT  , 2 , 1}, // NEGB
    {0x51, ILLEGAL_OP     , 0 , 1},
    {0x52, ILLEGAL_OP     , 0 , 1},
    {0x53, ADDR_INHERENT  , 2 , 1}, // COMB
    {0x54, ADDR_INHERENT  , 2 , 1}, // LSRB
    {0x55, ILLEGAL_OP     , 0 , 1},
    {0x56, ADDR_INHERENT  , 2 , 1}, // RORB
    {0x57, ADDR_INHERENT  , 2 , 1}, // ASRB
    {0x58, ADDR_INHERENT  , 2 , 1}, // LSLB
    {0x59, ADDR_INHERENT  , 2 , 1}, // ROLB
    {0x5a, ADDR_INHERENT  , 2 , 1}, // DECB
    {0x5b, ILLEGAL_OP     , 0 , 1},
    {0x5c, ADDR_INHERENT  , 2 , 1}, // INCB
    {0x5d, ADDR_INHERENT  , 2 , 1}, // TSTB
    {0x5e, ILLEGAL_OP     , 0 , 1},
    {0x5f, ADDR_INHERENT  , 2 , 1}, // CLRB

    {0x60, ADDR_INDEXED   , 6 , 2}, // NEG
    {0x61, ILLEGAL_OP     , 0 , 1},
    {0x62, ILLEGAL_OP     , 0 , 1},
    {0x63, ADDR_INDEXED   , 6 , 2}, // COM
    {0x64, ADDR_INDEXED   , 6 , 2}, // LSR
    {0x65, ADDR_INDEXED   , 6 , 2}, // LSL
    {0x66, ADDR_INDEXED   , 6 , 2}, // ROR
    {0x67, ADDR_INDEXED   , 6 , 2}, // ASR
    {0x68, ADDR_INDEXED   , 6 , 2}, // ASL
    {0x69, ADDR_INDEXED   , 6 , 2}, // ROL
    {0x6a, ADDR_INDEXED   , 6 , 2}, // DEC
    {0x6b, ILLEGAL_OP     , 0 , 1},
    {0x6c, ADDR_INDEXED   , 6 , 2}, // INC
    {0x6d, ADDR_INDEXED   , 6 , 2}, // TST
    {0x6e, ADDR_INDEXED   , 3 , 2}, // JMP
    {0x6f, ADDR_INDEXED   , 6 , 2}, // CLR

    {0x70, ADDR_EXTENDED  , 6 , 3}, // NEG
    {0x71, ILLEGAL_OP     , 0 , 1},
    {0x72, ILLEGAL_OP     , 0 , 1},
    {0x73, ADDR_EXTENDED  , 6 , 3}, // COM
    {0x74, ADDR_EXTENDED  , 6 , 3}, // LSR
    {0x75, ILLEGAL_OP     , 0 , 1},
    {0x76, ADDR_EXTENDED  , 6 , 3}, // ROR
    {0x77, ADDR_EXTENDED  , 6 , 3}, // ASR
    {0x78, ADDR_EXTENDED  , 6 , 3}, // ASL
    {0x79, ADDR_EXTENDED  , 6 , 3}, // ROL
    {0x7a, ADDR_EXTENDED  , 6 , 3}, // DEC
    {0x7b, ILLEGAL_OP     , 0 , 1},
    {0x7c, ADDR_EXTENDED  , 6 , 3}, // INC
    {0x7d, ADDR_EXTENDED  , 6 , 3}, // TST
    {0x7e, ADDR_EXTENDED  , 3 , 3}, // JMP
    {0x7f, ADDR_EXTENDED  , 6 , 3}, // CLR

    {0x80, ADDR_IMMEDIATE , 2 , 2}, // SUBA
    {0x81, ADDR_IMMEDIATE , 2 , 2}, // CMPA
    {0x82, ADDR_IMMEDIATE , 2 , 2}, // SBCA
    {0x83, ADDR_LIMMEDIATE, 4 , 3}, // SUBD
    {0x84, ADDR_IMMEDIATE , 2 , 2}, // ANDA
    {0x85, ADDR_IMMEDIATE , 2 , 2}, // BITA
    {0x86, ADDR_IMMEDIATE , 2 , 2}, // LDA
    {0x87, ILLEGAL_OP     , 0 , 1},
    {0x88, ADDR_IMMEDIATE , 2 , 2}, // EORA
    {0x89, ADDR_IMMEDIATE , 2 , 2}, // ADCA
    {0x8a, ADDR_IMMEDIATE , 2 , 2}, // ORA
    {0x8b, ADDR_IMMEDIATE , 2 , 2}, // ADDA
    {0x8c, ADDR_LIMMEDIATE, 4 , 3}, // CMPX
    {0x8d, ADDR_RELATIVE  , 6 , 2}, // BSR
    {0x8e, ADDR_LIMMEDIATE, 3 , 3}, // LDS
    {0x8f, ILLEGAL_OP     , 0 , 1},

    {0x90, ADDR_DIRECT    , 3 , 2}, // SUBA
    {0x91, ADDR_DIRECT    , 3 , 2}, // CMPA
    {0x92, ADDR_DIRECT    , 3 , 2}, // SBCA
    {0x93, ADDR_DIRECT    , 5 , 2}, // SUBD
    {0x94, ADDR_DIRECT    , 3 , 2}, // ANDA
    {0x95, ADDR_DIRECT    , 3 , 2}, // BITA
    {0x96, ADDR_DIRECT    , 3 , 2}, // LDA
    {0x97, ADDR_DIRECT    , 3 , 2}, // STA
    {0x98, ADDR_DIRECT    , 3 , 2}, // EORA
    {0x99, ADDR_DIRECT    , 3 , 2}, // ADCA
    {0x9a, ADDR_DIRECT    , 3 , 2}, // ORA
    {0x9b, ADDR_DIRECT    , 3 , 2}, // ADDA
    {0x9c, ADDR_DIRECT    , 5 , 2}, // CMPX
    {0x9d, ADDR_DIRECT    , 5 , 2}, // JSR
    {0x9e, ADDR_DIRECT    , 4 , 2}, // LDS
    {0x9f, ADDR_DIRECT    , 4 , 2}, // STS

    {0xa0, ADDR_INDEXED   , 4 , 2}, // SUBA
    {0xa1, ADDR_INDEXED   , 4 , 2}, // CMPA
    {0xa2, ADDR_INDEXED   , 4 , 2}, // SBCA
    {0xa3, ADDR_INDEXED   , 6 , 2}, // SUBD
    {0xa4, ADDR_INDEXED   , 4 , 2}, // ANDA
    {0xa5, ADDR_INDEXED   , 4 , 2}, // BITA
    {0xa6, ADDR_INDEXED   , 4 , 2}, // LDA
    {0xa7, ADDR_INDEXED   , 4 , 2}, // STA
    {0xa8, ADDR_INDEXED   , 4 , 2}, // EORA
    {0xa9, ADDR_INDEXED   , 4 , 2}, // ADCA
    {0xaa, ADDR_INDEXED   , 4 , 2}, // ORA
    {0xab, ADDR_INDEXED   , 4 , 2}, // ADDA
    {0xac, ADDR_INDEXED   , 6 , 2}, // CMPX
    {0xad, ADDR_INDEXED   , 6 , 2}, // JSR
    {0xae, ADDR_INDEXED   , 5 , 2}, // LDS
    {0xaf, ADDR_INDEXED   , 5 , 2}, // STS

    {0xb0, ADDR_EXTENDED  , 4 , 3}, // SUBA
    {0xb1, ADDR_EXTENDED  , 4 , 3}, // CMPA
    {0xb2, ADDR_EXTENDED  , 4 , 3}, // SBCA
    {0xb3, ADDR_EXTENDED  , 6 , 3}, // SUBD
    {0xb4, ADDR_EXTENDED  , 4 , 3}, // ANDA
    {0xb5, ADDR_EXTENDED  , 4 , 3}, // BITA
    {0xb6, ADDR_EXTENDED  , 4 , 3}, // LDA
    {0xb7, ADDR_EXTENDED  , 4 , 3}, // STA
    {0xb8, ADDR_EXTENDED  , 4 , 3}, // EORA
    {0xb9, ADDR_EXTENDED  , 4 , 3}, // ADCA
    {0xba, ADDR_EXTENDED  , 4 , 3}, // ORA
    {0xbb, ADDR_EXTENDED  , 4 , 3}, // ADDA
    {0xbc, ADDR_EXTENDED  , 6 , 3}, // CMPX
    {0xbd, ADDR_EXTENDED  , 6 , 3}, // JSR
    {0xbe, ADDR_EXTENDED  , 5 , 3}, // LDS
    {0xbf, ADDR_EXTENDED  , 5 , 3}, // STS

    {0xc0, ADDR_IMMEDIATE , 2 , 2}, // SUBB
    {0xc1, ADDR_IMMEDIATE , 2 , 2}, // CMPB
    {0xc2, ADDR_IMMEDIATE , 2 , 2}, // SBCB
    {0xc3, ADDR_LIMMEDIATE, 4 , 3}, // ADDD
    {0xc4, ADDR_IMMEDIATE , 2 , 2}, // ADDB
    {0xc5, ADDR_IMMEDIATE , 2 , 2}, // BITB
    {0xc6, ADDR_IMMEDIATE , 2 , 2}, // LDB
    {0xc7, ILLEGAL_OP     , 0 , 1},
    {0xc8, ADDR_IMMEDIATE , 2 , 2}, // EORB
    {0xc9, ADDR_IMMEDIATE , 2 , 2}, // ADCB
    {0xca, ADDR_IMMEDIATE , 2 , 2}, // ORB
    {0xcb, ADDR_IMMEDIATE , 2 , 2}, // ADDB
    {0xcc, ADDR_LIMMEDIATE, 3 , 3}, // LDD
    {0xcd, ILLEGAL_OP     , 0 , 1},
    {0xce, ADDR_LIMMEDIATE, 3 , 3}, // LDX
    {0xcf, ILLEGAL_OP     , 0 , 1},

    {0xd0, ADDR_DIRECT    , 3 , 2}, // SUBB
    {0xd1, ADDR_DIRECT    , 3 , 2}, // CMPB
    {0xd2, ADDR_DIRECT    , 3 , 2}, // SBCB
    {0xd3, ADDR_DIRECT    , 5 , 2}, // ADDD
    {0xd4, ADDR_DIRECT    , 3 , 2}, // ADDB
    {0xd5, ADDR_DIRECT    , 3 , 2}, // BITB
    {0xd6, ADDR_DIRECT    , 3 , 2}, // LDB
    {0xd7, ADDR_DIRECT    , 3 , 2}, // STB
    {0xd8, ADDR_DIRECT    , 3 , 2}, // EORB
    {0xd9, ADDR_DIRECT    , 3 , 2}, // ADCB
    {0xda, ADDR_DIRECT    , 3 , 2}, // ORB
    {0xdb, ADDR_DIRECT    , 3 , 2}, // ADDB
    {0xdc, ADDR_DIRECT    , 4 , 2}, // LDD
    {0xdd, ADDR_DIRECT    , 4 , 2}, // STD
    {0xde, ADDR_DIRECT    , 4 , 2}, // LDX
    {0xdf, ADDR_DIRECT    , 4 , 2}, // STX

    {0xe0, ADDR_INDEXED   , 4 , 2}, // SUBB
    {0xe1, ADDR_INDEXED   , 4 , 2}, // CMPB
    {0xe2, ADDR_INDEXED   , 4 , 2}, // SBCB
    {0xe3, ADDR_INDEXED   , 6 , 2}, // ADDD
    {0xe4, ADDR_INDEXED   , 4 , 2}, // ADDB
    {0xe5, ADDR_INDEXED   , 4 , 2}, // BITB
    {0xe6, ADDR_INDEXED   , 4 , 2}, // LDB
    {0xe7, ADDR_INDEXED   , 4 , 2}, // STB
    {0xe8, ADDR_INDEXED   , 4 , 2}, // EORB
    {0xe9, ADDR_INDEXED   , 4 , 2}, // ADCB
    {0xea, ADDR_INDEXED   , 4 , 2}, // ORB
    {0xeb, ADDR_INDEXED   , 4 , 2}, // ADDB
    {0xec, ADDR_INDEXED   , 5 , 2}, // LDD
    {0xed, ADDR_INDEXED   , 5 , 2}, // STD
    {0xee, ADDR_INDEXED   , 5 , 2}, // LDX
    {0xef, ADDR_INDEXED   , 5 , 2}, // STX

    {0xf0, ADDR_EXTENDED  , 4 , 3}, // SUBB
    {0xf1, ADDR_EXTENDED  , 4 , 3}, // CMPB
    {0xf2, ADDR_EXTENDED  , 4 , 3}, // SBCB
    {0xf3, ADDR_EXTENDED  , 6 , 3}, // ADDD
    {0xf4, ADDR_EXTENDED  , 4 , 3}, // ADDB
    {0xf5, ADDR_EXTENDED  , 4 , 3}, // BITB
    {0xf6, ADDR_EXTENDED  , 4 , 3}, // LDB
    {0xf7, ADDR_EXTENDED  , 4 , 3}, // STB
    {0xf8, ADDR_EXTENDED  , 4 , 3}, // EORB
    {0xf9, ADDR_EXTENDED  , 4 , 3}, // ADCB
    {0xfa, ADDR_EXTENDED  , 4 , 3}, // ORB
    {0xfb, ADDR_EXTENDED  , 4 , 3}, // ADDB
    {0xfc, ADDR_EXTENDED  , 5 , 3}, // LDD
    {0xfd, ADDR_EXTENDED  , 5 , 3}, // STD
    {0xfe, ADDR_EXTENDED  , 5 , 3}, // LDX
    {0xff, ADDR_EXTENDED  , 5 , 3}  // STX
};

#endif  /* __MC6803_H__ */
