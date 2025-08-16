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


/********************************************************************
 * cpu.c
 *
 *  MC6803 CPU emulation module.
 *
 *******************************************************************/
#include    <nds.h>
#include    <string.h>

#include    "mc6803.h"
#include    "mem.h"
#include    "cpu.h"

#define CPU_CYCLES_PER_LINE             57

/* -----------------------------------------
   Local definitions
----------------------------------------- */

/* MC6803 vector addresses, where
 * each vector is two bytes long.
 */
#define     VEC_RESET               0xfffe
#define     VEC_NMI                 0xfffc
#define     VEC_SWI                 0xfffa
#define     VEC_IRQ                 0xfff8
#define     VEC_ICF                 0xfff6
#define     VEC_OCF                 0xfff4
#define     VEC_TOF                 0xfff2
#define     VEC_SCI                 0xfff0

/* Condition-code register bit
 */
#define     CC_FLAG_CLR             0
#define     CC_FLAG_SET             1

/* Word and Byte operations
 */
#define     GET_REG_HIGH(r)         ((uint8_t)(r >> 8))
#define     GET_REG_LOW(r)          ((uint8_t)r)
#define     SIG_EXTEND(b)           ((((uint8_t)b) & 0x80) ? (((uint16_t)b) | 0xff00):((uint16_t)b))

/* -----------------------------------------
   Module functions
----------------------------------------- */

/* CPU op-code processing.
 * These functions manipulate global variable,
 * CPU registers and CC flags.
 */
uint8_t adc(uint8_t acc, uint8_t byte);
uint8_t add(uint8_t acc, uint8_t byte);
void    addd(uint16_t word);
uint8_t and(uint8_t acc, uint8_t byte);
uint8_t asl(uint8_t byte);
uint8_t asr(uint8_t byte);
void    bit(uint8_t acc, uint8_t byte);
uint8_t clr(void);
void    cmp(uint8_t arg, uint8_t byte);
void    cmp16(uint16_t arg, uint16_t word);
uint8_t com(uint8_t byte);
void    wai(void);
void    daa(void);
uint8_t dec(uint8_t byte);
uint8_t eor(uint8_t acc, uint8_t byte);
uint8_t inc(uint8_t byte);
uint8_t lsr(uint8_t byte);
uint8_t lsl(uint8_t byte);
uint16_t lsr16(uint16_t word);
uint16_t lsl16(uint16_t word);
uint8_t lsl(uint8_t byte);
uint8_t neg(uint8_t byte);
uint8_t ngc(uint8_t byte);
uint8_t or(uint8_t acc, uint8_t byte);
void    orcc(uint8_t byte);
uint8_t rol(uint8_t byte);
uint8_t ror(uint8_t byte);
void    rti(void);
uint8_t sbc(uint8_t acc, uint8_t byte);
uint8_t sub(uint8_t acc, uint8_t byte);
void    subd(uint16_t word);
void    swi(void);
void    tst(uint8_t byte);

/* CPU op-code support functions
 */
int     get_eff_addr(int op_code);

/* Condition code register CC functions
 */
void    eval_cc_c(uint16_t value);
void    eval_cc_c16(uint32_t value);
void    eval_cc_z(uint16_t value);
void    eval_cc_z16(uint32_t value);
void    eval_cc_n(uint16_t value);
void    eval_cc_n16(uint32_t value);
void    eval_cc_v(uint8_t val1, uint8_t val2, uint16_t result);
void    eval_cc_v16(uint16_t val1, uint16_t val2, uint32_t result);
void    eval_cc_h(uint8_t val1, uint8_t val2, uint8_t result);

uint8_t get_cc(void);
void    set_cc(uint8_t value);

/* -----------------------------------------
   Module globals
----------------------------------------- */

/* MC6803 register file
 */
cpu_state_t cpu  __attribute__((section(".dtcm")));

struct cc_t
{
    uint8_t c;
    uint8_t v;
    uint8_t z;
    uint8_t n;
    uint8_t i;
    uint8_t h;
} cc __attribute__((section(".dtcm")));

int cycles_this_scanline    __attribute__((section(".dtcm"))) = 0;

/*------------------------------------------------
 * cpu_init()
 *
 *  Initialize the CPU for command execution at address.
 *  Function should be called once before cpu_run() or cpu_single_step().
 */
void cpu_init(void)
{
    /* Registers
     */
    cpu.x  = 0;
    cpu.sp = 0;
    cpu.pc = 0;
    cpu.ab.ab.a  = 0;
    cpu.ab.ab.b  = 0;

    set_cc(0);

    /* CPU state
     */
    cpu.reset_asserted  = 0;
    cpu.cpu_state       = CPU_HALTED;
    
    cpu.counter         = 0;
    cpu.compare         = 0xffff;

    // And set the PC to where we want to start
    cpu.pc = (mem_read(VEC_RESET) << 8) + mem_read(VEC_RESET+1);
}

/*------------------------------------------------
 * cpu_reset()
 *
 *  Assert RESET state
 *
 *  param:  0- clear, 1- asserted
 *  return: Nothing
 */
void cpu_reset(int state)
{
    cpu.reset_asserted = state;
}

void cpu_check_reset(void)
{
    if ( cpu.reset_asserted )
    {
        cycles_this_scanline = 0;
        cc.i = CC_FLAG_SET;
        cpu.cpu_state = CPU_RESET;
        cpu.pc = (mem_read(VEC_RESET) << 8) + mem_read(VEC_RESET+1);
        cpu.reset_asserted = 0;
        cpu.cpu_state = CPU_EXEC;
    }
}

/*------------------------------------------------
 * cpu_run()
 *
 *  Start CPU.
 *  Function should be called periodically
 *  after an initialization by cpu_run_init().
 *
 *  param:  Nothing
 *  return: Nothing
 */
int trace=0;
int zzz=0;
ITCM_CODE void cpu_run(void)
{
    int         eff_addr;
    uint8_t     operand8;
    uint16_t    operand16;
    int         op_code;

    while (1)
    {
        if (cpu.cpu_state) // Something OTHER than CPU_EXEC - might be CPU halted (WAI instruction) or a CPU Exception
        {
            if (cpu.cpu_state == CPU_HALTED)
            {
                debug[2]++;
                // If we are halted, we still clock the timers...
                for (int i=0; i<CPU_CYCLES_PER_LINE; i++)
                {
                    if (++cpu.counter == cpu.compare)
                    {
                        Memory[0x08] |= TCSR_OCF;
                    }
                    if (cpu.counter & 0xFFFF0000) // Overflow
                    {
                        cpu.counter &= 0xFFFF;
                        Memory[0x08] |= TCSR_TOF;
                    }
                }
                
                if ( !(cc.i) && (Memory[0x08] & TCSR_TOF) && (Memory[0x08] & TCSR_ETOI) )
                {
                    cpu.cpu_state = CPU_EXEC;
                    cc.i = CC_FLAG_SET;  // No more interrupts until cleared
                    cpu.pc = (mem_read(VEC_TOF) << 8) + mem_read(VEC_TOF+1);
                }
                else
                if ( !(cc.i) && (Memory[0x08] & TCSR_OCF) && (Memory[0x08] & TCSR_EOCI) )
                {
                    cpu.cpu_state = CPU_EXEC;
                    cc.i = CC_FLAG_SET;  // No more interrupts until cleared
                    cpu.pc = (mem_read(VEC_OCF) << 8) + mem_read(VEC_OCF+1);
                }
                else // Return
                {
                    return;
                }
            }
            
            if (cpu.cpu_state == CPU_EXCEPTION)
            {
                debug[4]++;
                return;
            }
        }

        if (Memory[0x08] & (TCSR_TOF | TCSR_OCF))   // Is either supported interrupt flag set?
        {
            /* If an interrupt is received and it is enabled, then setup stack frame and call interrupt service by
             * setting the PC to the vectors content.
             */
            if ( !(cc.i) && (Memory[0x08] & TCSR_TOF) && (Memory[0x08] & TCSR_ETOI) )
            {
                debug[0]++;
                cpu.cpu_state = CPU_EXEC;
                cycles_this_scanline += 12;

                mem_write(cpu.sp, (cpu.pc >> 0) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, (cpu.pc >> 8) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, (cpu.x  >> 0) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, (cpu.x  >> 8) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, cpu.ab.ab.a);
                cpu.sp--;
                mem_write(cpu.sp, cpu.ab.ab.b);
                cpu.sp--;
                mem_write(cpu.sp, get_cc());
                cpu.sp--;
                                
                cc.i = CC_FLAG_SET;  // No more interrupts until cleared
                cpu.pc = (mem_read(VEC_TOF) << 8) + mem_read(VEC_TOF+1);
            }

            if ( !(cc.i) && (Memory[0x08] & TCSR_OCF) && (Memory[0x08] & TCSR_EOCI) )
            {
                debug[1]++;
                cpu.cpu_state = CPU_EXEC;
                cycles_this_scanline += 12;

                mem_write(cpu.sp, (cpu.pc >> 0) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, (cpu.pc >> 8) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, (cpu.x  >> 0) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, (cpu.x  >> 8) & 0xff);
                cpu.sp--;
                mem_write(cpu.sp, cpu.ab.ab.a);
                cpu.sp--;
                mem_write(cpu.sp, cpu.ab.ab.b);
                cpu.sp--;
                mem_write(cpu.sp, get_cc());
                cpu.sp--;
                
                cc.i = CC_FLAG_SET;  // No more interrupts until cleared
                cpu.pc = (mem_read(VEC_OCF) << 8) + mem_read(VEC_OCF+1);
            }
        }

        // Fetch the OP Code directly from memory
        op_code = mem_read_pc(cpu.pc++);
        
#if 0
        //if (cpu.pc == 0x698B) trace=1;
        //if (keysCurrent() & KEY_X) trace = 1;   
        if (trace)
        {
            if (++zzz < 5000)
            {
                debug_printf("%04X %02X %04s [%02X A=%02X, B=%02X, X=%04X, SP=%04X CC=%02X]\n", cpu.pc-1, op_code, op_name[op_code], Memory[cpu.pc], cpu.ab.ab.a, cpu.ab.ab.b, cpu.x, cpu.sp, get_cc());
            }
        }
#endif        

        // Process the Op-Code...
        {
            /* 'operand8' will be operand byte, and for a 16-bit operand 'operand8'
             * will be the high order byte and low order byte should be read separately
             * and combined into 16-bit value.
             */
            cycles_this_scanline += machine_code[op_code].cycles;
            
            // --------------------------------------------------------------
            // Counters and clocks... this is where we handle the CPU timer
            // module with the free-running timer counter and a compare reg.
            // --------------------------------------------------------------
            if (cpu.counter < cpu.compare)
            {
                if ((cpu.counter + machine_code[op_code].cycles) >= cpu.compare)
                {
                    Memory[0x08] |= TCSR_OCF;
                }
            }
            cpu.counter += machine_code[op_code].cycles;
            if (cpu.counter & 0xFFFF0000) // Overflow
            {
                cpu.counter &= 0xFFFF;
                Memory[0x08] |= TCSR_TOF;
            }

            eff_addr = get_eff_addr(machine_code[op_code].mode);
            
            switch ( op_code )
            {
                // ABA
                case 0x1B:
                    cpu.ab.ab.a = add(cpu.ab.ab.a, cpu.ab.ab.b);
                    break;

                // ABX
                case 0x3a:
                    cpu.x += cpu.ab.ab.b;
                    break;

                // ADCA
                case 0x89:
                case 0x99:
                case 0xa9:
                case 0xb9:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = adc(cpu.ab.ab.a, operand8);
                    break;

                // ADCB
                case 0xc9:
                case 0xd9:
                case 0xe9:
                case 0xf9:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = adc(cpu.ab.ab.b, operand8);
                    break;

                // ADDA
                case 0x8b:
                case 0x9b:
                case 0xab:
                case 0xbb:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = add(cpu.ab.ab.a, operand8);
                    break;

                // ADDB
                case 0xcb:
                case 0xdb:
                case 0xeb:
                case 0xfb:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = add(cpu.ab.ab.b, operand8);
                    break;

                // ADDD
                case 0xc3:
                case 0xd3:
                case 0xe3:
                case 0xf3:
                    operand8 = (uint8_t) mem_read(eff_addr++);
                    operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    addd(operand16);
                    break;

                // ANDA
                case 0x84:
                case 0x94:
                case 0xa4:
                case 0xb4:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = and(cpu.ab.ab.a, operand8);
                    break;

                // ADDB
                case 0xc4:
                case 0xd4:
                case 0xe4:
                case 0xf4:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = and(cpu.ab.ab.b, operand8);
                    break;

                // BITA
                case 0x85:
                case 0x95:
                case 0xa5:
                case 0xb5:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    bit(cpu.ab.ab.a, operand8);
                    break;

                // BITB
                case 0xc5:
                case 0xd5:
                case 0xe5:
                case 0xf5:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    bit(cpu.ab.ab.b, operand8);
                    break;
                    
                // CBA
                case 0x11:
                    cmp(cpu.ab.ab.a, cpu.ab.ab.b);
                    break;

                // CLR
                case 0x6f:
                case 0x7f:
                    operand8 = clr();
                    mem_write(eff_addr, operand8);
                    break;
                
                // CLRA
                case 0x4f:
                    cpu.ab.ab.a = clr();
                    break;
                
                // CLRB
                case 0x5f:
                    cpu.ab.ab.b = clr();
                    break;

                // CMPA
                case 0x81:
                case 0x91:
                case 0xa1:
                case 0xb1:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cmp(cpu.ab.ab.a, operand8);
                    break;

                // CMPB
                case 0xc1:
                case 0xd1:
                case 0xe1:
                case 0xf1:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cmp(cpu.ab.ab.b, operand8);
                    break;

                // CMPX
                case 0x8c:
                case 0x9c:
                case 0xac:
                case 0xbc:
                    operand8 = (uint8_t) mem_read(eff_addr++);
                    operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    cmp16(cpu.x, operand16);
                    break;

                // COM
                case 0x63:
                case 0x73:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = com(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // COMA
                case 0x43:
                    cpu.ab.ab.a = com(cpu.ab.ab.a);
                    break;
                    
                // COMB
                case 0x53:
                    cpu.ab.ab.b = com(cpu.ab.ab.b);
                    break;

                // DAA
                case 0x19:
                    daa();
                    break;

                // DEC
                case 0x6a:
                case 0x7a:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = dec(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // DECA
                case 0x4a:
                    cpu.ab.ab.a = dec(cpu.ab.ab.a);
                    break;

                // DECB
                case 0x5a:
                    cpu.ab.ab.b = dec(cpu.ab.ab.b);
                    break;

                // EORA
                case 0x88:
                case 0x98:
                case 0xa8:
                case 0xb8:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = eor(cpu.ab.ab.a, operand8);
                    break;

                // EORB
                case 0xc8:
                case 0xd8:
                case 0xe8:
                case 0xf8:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = eor(cpu.ab.ab.b, operand8);
                    break;

                // INC
                case 0x6c:
                case 0x7c:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = inc(operand8);
                    mem_write(eff_addr, operand8);
                    break;
                    
                // INCA
                case 0x4c:
                    cpu.ab.ab.a = inc(cpu.ab.ab.a);
                    break;

                // INCB
                case 0x5c:
                    cpu.ab.ab.b = inc(cpu.ab.ab.b);
                    break;
                    
                // LDA
                case 0x86:
                case 0x96:
                case 0xa6:
                case 0xb6:
                    cpu.ab.ab.a = (uint8_t) mem_read(eff_addr);
                    eval_cc_z((uint16_t) cpu.ab.ab.a);
                    eval_cc_n((uint16_t) cpu.ab.ab.a);
                    cc.v = CC_FLAG_CLR;
                    break;

                // LDB
                case 0xc6:
                case 0xd6:
                case 0xe6:
                case 0xf6:
                    cpu.ab.ab.b = (uint8_t) mem_read(eff_addr);
                    eval_cc_z((uint16_t) cpu.ab.ab.b);
                    eval_cc_n((uint16_t) cpu.ab.ab.b);
                    cc.v = CC_FLAG_CLR;
                    break;

                // LDD/LDAD
                case 0xcc:
                case 0xdc:
                case 0xec:
                case 0xfc:
                    cpu.ab.ab.a = (uint8_t) mem_read(eff_addr++);
                    cpu.ab.ab.b = (uint8_t) mem_read(eff_addr);
                    eval_cc_z16(cpu.ab.d);
                    eval_cc_n16(cpu.ab.d);
                    cc.v = CC_FLAG_CLR;
                    break;                    

                // LSL
                case 0x65:
                case 0x78:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = lsl(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // LSLA
                case 0x48:
                    cpu.ab.ab.a = lsl(cpu.ab.ab.a);
                    break;

                // LSLB
                case 0x58:
                    cpu.ab.ab.b = lsl(cpu.ab.ab.b);
                    break;
                    
                // ASL
                case 0x68:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = asl(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // ASR
                case 0x67:
                case 0x77:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = asr(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // ASRA
                case 0x47:
                    cpu.ab.ab.a = asr(cpu.ab.ab.a);
                    break;
                
                // ASRB
                case 0x57:
                    cpu.ab.ab.b = asr(cpu.ab.ab.b);
                    break;                                        

                // LSLD/ASLD
                case 0x05:
                    cpu.ab.d = lsl16(cpu.ab.d);
                    break;

                // LSR
                case 0x64:
                case 0x74:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = lsr(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // LSRA
                case 0x44:
                    cpu.ab.ab.a = lsr(cpu.ab.ab.a);
                    break;

                // LSRB
                case 0x54:
                    cpu.ab.ab.b = lsr(cpu.ab.ab.b);
                    break;

                // LSRD
                case 0x04:
                    cpu.ab.d = lsr16(cpu.ab.d);
                    break;

                // MUL
                case 0x3d:
                    operand16 = cpu.ab.ab.a * cpu.ab.ab.b;
                    cpu.ab.ab.a = GET_REG_HIGH(operand16);
                    cpu.ab.ab.b = GET_REG_LOW(operand16);
                    cc.c = (cpu.ab.ab.b & 0x80) ? CC_FLAG_SET : CC_FLAG_CLR;
                    break;

                // NEG
                case 0x60:
                case 0x70:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = neg(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // NEGA
                case 0x40:
                    cpu.ab.ab.a = neg(cpu.ab.ab.a);
                    break;
                    
                // NEGB
                case 0x50:
                    cpu.ab.ab.b = neg(cpu.ab.ab.b);
                    break;

                // NOP
                case 0x01:
                    break;
                
                // ORA
                case 0x8a:
                case 0x9a:
                case 0xaa:
                case 0xba:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = or(cpu.ab.ab.a, operand8);
                    break;

                // ORB
                case 0xca:
                case 0xda:
                case 0xea:
                case 0xfa:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = or(cpu.ab.ab.b, operand8);
                    break;

                // PSHA
                case 0x36:
                    mem_write(cpu.sp, cpu.ab.ab.a);
                    cpu.sp--;
                    break;

                // PULA
                case 0x32:
                    cpu.sp++;
                    cpu.ab.ab.a = mem_read(cpu.sp);
                    break;

                // PSHB
                case 0x37:
                    mem_write(cpu.sp, cpu.ab.ab.b);
                    cpu.sp--;
                    break;

                // PULB
                case 0x33:
                    cpu.sp++;
                    cpu.ab.ab.b = mem_read(cpu.sp);
                    break;

                // ROL
                case 0x69:
                case 0x79:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = rol(operand8);
                    mem_write(eff_addr, operand8);
                    break;
                    
                // ROLA
                case 0x49:
                    cpu.ab.ab.a = rol(cpu.ab.ab.a);
                    break;

                // ROLB
                case 0x59:
                    cpu.ab.ab.b = rol(cpu.ab.ab.b);
                    break;

                // ROR
                case 0x66:
                case 0x76:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = ror(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                // RORA
                case 0x46:
                    cpu.ab.ab.a = ror(cpu.ab.ab.a);
                    break;

                // RORB
                case 0x56:
                    cpu.ab.ab.b = ror(cpu.ab.ab.b);
                    break;

                // SBA
                case 0x10:
                    cpu.ab.ab.a = sub(cpu.ab.ab.a, cpu.ab.ab.b);
                    break;
                    
                // SBCA
                case 0x82:
                case 0x92:
                case 0xa2:
                case 0xb2:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = sbc(cpu.ab.ab.a, operand8);
                    break;

                // SBCB
                case 0xc2:
                case 0xd2:
                case 0xe2:
                case 0xf2:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = sbc(cpu.ab.ab.b, operand8);
                    break;

                // STA
                case 0x97:
                case 0xa7:
                case 0xb7:
                    mem_write(eff_addr, cpu.ab.ab.a);
                    eval_cc_z((uint16_t) cpu.ab.ab.a);
                    eval_cc_n((uint16_t) cpu.ab.ab.a);
                    cc.v = CC_FLAG_CLR;
                    break;

                // STB
                case 0xd7:
                case 0xe7:
                case 0xf7:
                    mem_write(eff_addr, cpu.ab.ab.b);
                    eval_cc_z((uint16_t) cpu.ab.ab.b);
                    eval_cc_n((uint16_t) cpu.ab.ab.b);
                    cc.v = CC_FLAG_CLR;
                    break;

                // STD/STAD
                case 0xdd:
                case 0xed:
                case 0xfd:
                    mem_write(eff_addr, cpu.ab.ab.a);
                    mem_write(eff_addr + 1, cpu.ab.ab.b);
                    eval_cc_z16(cpu.ab.d);
                    eval_cc_n16(cpu.ab.d);
                    cc.v = CC_FLAG_CLR;
                    break;

                // SUBA
                case 0x80:
                case 0x90:
                case 0xa0:
                case 0xb0:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.a = sub(cpu.ab.ab.a, operand8);
                    break;

                // SUBB
                case 0xc0:
                case 0xd0:
                case 0xe0:
                case 0xf0:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.ab.ab.b = sub(cpu.ab.ab.b, operand8);
                    break;

                // SUBD
                case 0x83:
                case 0x93:
                case 0xa3:
                case 0xb3:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    operand16 = ((uint16_t ) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    subd(operand16);
                    break;

                // TAB
                case 0x16:
                    cpu.ab.ab.b = cpu.ab.ab.a;
                    eval_cc_z((uint16_t) cpu.ab.ab.b);
                    eval_cc_n((uint16_t) cpu.ab.ab.b);
                    cc.v = CC_FLAG_CLR;
                    break;

                // TBA
                case 0x17:
                    cpu.ab.ab.a = cpu.ab.ab.b;
                    eval_cc_z((uint16_t) cpu.ab.ab.a);
                    eval_cc_n((uint16_t) cpu.ab.ab.a);
                    cc.v = CC_FLAG_CLR;
                    break;

                // TST
                case 0x6d:
                case 0x7d:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    tst(operand8);
                    break;

                // TSTA
                case 0x4d:
                    tst(cpu.ab.ab.a);
                    break;

                // TSTB
                case 0x5d:
                    tst(cpu.ab.ab.b);
                    break;

                // BRA
                case 0x20:
                    cpu.pc = eff_addr;
                    break;

                // BRN
                case 0x21:
                    // Branch never
                    break;

                // BCC
                case 0x24:
                    if ( cc.c == CC_FLAG_CLR ) cpu.pc = eff_addr;
                    break;

                // BCS
                case 0x25:
                    if ( cc.c == CC_FLAG_SET ) cpu.pc = eff_addr;
                    break;

                // BEQ
                case 0x27:
                    if ( cc.z == CC_FLAG_SET ) cpu.pc = eff_addr;
                    break;

                // BNE
                case 0x26:
                    if ( cc.z == CC_FLAG_CLR ) cpu.pc = eff_addr;
                    break;

                // BGE
                case 0x2c:
                    if ( cc.n == cc.v ) cpu.pc = eff_addr;
                    break;

                // BLT
                case 0x2d:
                    if ( cc.n != cc.v ) cpu.pc = eff_addr;
                    break;

                // BGT
                case 0x2e:
                    if ( cc.n == cc.v && cc.z == CC_FLAG_CLR ) cpu.pc = eff_addr;
                    break;

                // BHI
                case 0x22:
                    if ( cc.c == CC_FLAG_CLR && cc.z == CC_FLAG_CLR ) cpu.pc = eff_addr;
                    break;

                // BLE
                case 0x2f:
                    if ( cc.n != cc.v || cc.z == CC_FLAG_SET ) cpu.pc = eff_addr;
                    break;

                // BLS
                case 0x23:
                    if ( cc.c == CC_FLAG_SET || cc.z == CC_FLAG_SET ) cpu.pc = eff_addr;
                    break;

                // BMI
                case 0x2b:
                    if ( cc.n == CC_FLAG_SET ) cpu.pc = eff_addr;
                    break;

                // BPL
                case 0x2a:
                    if ( cc.n == CC_FLAG_CLR ) cpu.pc = eff_addr;
                    break;

                // BVS
                case 0x29:
                    if ( cc.v == CC_FLAG_SET ) cpu.pc = eff_addr;
                    break;
                    
                // BVC
                case 0x28:
                    if ( cc.v == CC_FLAG_CLR ) cpu.pc = eff_addr;
                    break;

                // BSR
                case 0x8d:
                    mem_write(cpu.sp, GET_REG_LOW(cpu.pc));
                    cpu.sp--;
                    mem_write(cpu.sp, GET_REG_HIGH(cpu.pc));
                    cpu.sp--;
                    cpu.pc = eff_addr;
                    break;

                // JMP
                case 0x6e:
                case 0x7e:
                    cpu.pc = eff_addr;
                    break;

                // JSR
                case 0x9d:
                case 0xad:
                case 0xbd:
                    mem_write(cpu.sp, GET_REG_LOW(cpu.pc));
                    cpu.sp--;
                    mem_write(cpu.sp, GET_REG_HIGH(cpu.pc));
                    cpu.sp--;
                    cpu.pc = eff_addr;
                    break;

                // RTI
                case 0x3b:
                    rti();
                    break;

                // RTS
                case 0x39:
                     /* Restore PC and return
                      */
                     cpu.sp++;
                     cpu.pc = (uint16_t) mem_read(cpu.sp) << 8;
                     cpu.sp++;
                     cpu.pc += mem_read(cpu.sp);
                     break;

                // DEX
                case 0x09:
                    cpu.x--;
                    eval_cc_z16(cpu.x);
                    break;

                // INX
                case 0x08:
                    cpu.x++;
                    eval_cc_z16(cpu.x);
                    break;

                // LDX
                case 0xce:
                case 0xde:
                case 0xee:
                case 0xfe:
                    cpu.x = (uint16_t) mem_read(eff_addr) << 8;
                    cpu.x += mem_read(eff_addr+1);
                    eval_cc_z16(cpu.x);
                    eval_cc_n16(cpu.x);
                    cc.v = CC_FLAG_CLR;
                    break;
                    
                // STX
                case 0xdf:
                case 0xef:
                case 0xff:
                    mem_write(eff_addr, (uint8_t) (cpu.x >> 8));
                    mem_write(eff_addr + 1, (uint8_t) (cpu.x));
                    eval_cc_z16(cpu.x);
                    eval_cc_n16(cpu.x);
                    cc.v = CC_FLAG_CLR;
                    break;
                    
                // PSHX
                case 0x3c:
                    mem_write(cpu.sp, (uint8_t) (cpu.x));
                    cpu.sp--;
                    mem_write(cpu.sp, (uint8_t) (cpu.x >> 8));
                    cpu.sp--;
                    break;

                // PULX
                case 0x38:
                    cpu.sp++;
                    cpu.x = (uint16_t)mem_read(cpu.sp) << 8;
                    cpu.sp++;
                    cpu.x += mem_read(cpu.sp);
                    break;

                // TXS
                case 0x35:
                    cpu.sp = cpu.x - 1;
                    break;

                // TSX
                case 0x30:
                    cpu.x = cpu.sp + 1;
                    break;

                // DES
                case 0x34:
                    cpu.sp--;
                    break;

                // INS
                case 0x31:
                    cpu.sp++;
                    break;

                // LDS
                case 0x8e:
                case 0x9e:
                case 0xae:
                case 0xbe:
                    cpu.sp = (uint16_t) mem_read(eff_addr) << 8;
                    cpu.sp += mem_read(eff_addr + 1);
                    eval_cc_z16(cpu.sp);
                    eval_cc_n16(cpu.sp);
                    cc.v = CC_FLAG_CLR;
                    break;

                // STS
                case 0x9f:
                case 0xaf:
                case 0xbf:
                    mem_write(eff_addr, (uint8_t) (cpu.sp >> 8));
                    mem_write(eff_addr + 1, (uint8_t) (cpu.sp));
                    eval_cc_z16(cpu.sp);
                    eval_cc_n16(cpu.sp);
                    cc.v = CC_FLAG_CLR;
                    break;

                // CLC
                case 0x0c:
                    cc.c = CC_FLAG_CLR;
                    break;

                // CLI
                case 0x0e:
                    cc.i = CC_FLAG_CLR;
                    break;

                // CLV
                case 0x0a:
                    cc.v = CC_FLAG_CLR;
                    break;

                // SEC
                case 0x0d:
                    cc.c = CC_FLAG_SET;
                    break;

                // SEI
                case 0x0f:
                    cc.i = CC_FLAG_SET;
                    break;

                // SEV
                case 0x0b:
                    cc.v = CC_FLAG_SET;
                    break;
                    
                // TAP
                case 0x06:
                    set_cc(cpu.ab.ab.a);
                    break;

                // TPA
                case 0x07:
                    cpu.ab.ab.a = get_cc();
                    break;
                    
                // WAI
                case 0x3e:
                    wai();
                    break;

                // SWI
                case 0x3f:
                    swi();
                    break;
                    
                // Undocumented: CLB - Clear B
                case 0x00:
                    cpu.ab.ab.b = 0; // Flags not affected
                    break;

                // Undocumented: SEXA
                case 0x02:
                    cpu.ab.ab.a = (cc.c ? 0xFF:0x00);
                    break;
                
                // Undocumented: SETA    
                case 0x03:
                    cpu.ab.ab.a = 0xFF;
                    break;

                // Undocumented: NGC - Negate with Carry A
                case 0x42:
                    cpu.ab.ab.a = ngc(cpu.ab.ab.a);
                    break;
                
                // Undocumented: NGC - Negate with Carry B    
                case 0x52:
                    cpu.ab.ab.b = ngc(cpu.ab.ab.b);
                    break;
                    
                // Undocumented: NGC - Negate with Carry
                case 0x62:
                case 0x72:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = ngc(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                default:
                    /* Exception: Illegal op-code cpu_run()
                     */
                    if (debug[7] == 0) {debug[7] = op_code;}
                    cpu.cpu_state = CPU_EXCEPTION;

            }
        }

        if (cycles_this_scanline >= CPU_CYCLES_PER_LINE)
        {
            cycles_this_scanline -= CPU_CYCLES_PER_LINE;
            break;
        }
    }
}

/*------------------------------------------------
 * adc()
 *
 *  Add with carry.
 *
 *  acc+byte+carry
 */
inline __attribute__((always_inline)) uint8_t adc(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = (acc + byte + cc.c);

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, byte, result);
    eval_cc_h(acc, byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * add()
 *
 *  Add.
 *
 *  acc+byte
 */
inline __attribute__((always_inline)) uint8_t add(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = (acc + byte);

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, byte, result);
    eval_cc_h(acc, byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * addd()
 *
 *  Add 16-bit operand to Acc-D
 *
 *  acc+word
 */
inline __attribute__((always_inline)) void addd(uint16_t word)
{
    uint16_t acc;
    uint32_t result;

    acc = (cpu.ab.ab.a << 8) + cpu.ab.ab.b;
    result = (uint32_t)acc + (uint32_t)word;

    cpu.ab.ab.a = (result >> 8) & 0xff;
    cpu.ab.ab.b = result & 0xff;

    eval_cc_c16(result);
    eval_cc_z16(result);
    eval_cc_v16(acc, word, result);
    eval_cc_n16(result);
}

/*------------------------------------------------
 * and()
 *
 *  Logical AND accumulator with byte operand.
 *
 */
inline __attribute__((always_inline)) uint8_t and(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = (acc & byte);

    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = CC_FLAG_CLR;

    return result;
}


/*------------------------------------------------
 * asl()
 *
 *  Arithmetic shift left.
 *
 *  'byte' shift left, MSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t asl(uint8_t byte)
{
    uint16_t result;

    result = ((uint16_t) byte) << 1;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;

    return (uint8_t) result;
}

/*------------------------------------------------
 * asr()
 *
 *  Arithmetic shift right.
 *
 *  'byte' shift right, MSB replicated b7, LSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t asr(uint8_t byte)
{
    uint8_t result;

    result = (byte >> 1) | (byte & 0x80);

    cc.c = byte & 0x01 ? CC_FLAG_SET : CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * bit()
 *
 *  Bit test accumulator with operand by AND
 *  without changing accumulator.
 *  Change flag bit appropriately.
 *
 *  acc AND byte
 */
inline __attribute__((always_inline))  void bit(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = acc & byte;

    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = CC_FLAG_CLR;
}

/*------------------------------------------------
 * clr()
 *
 *  Clear (zero) bits in operand.
 *  Change flag bit appropriately.
 *
 */
inline __attribute__((always_inline)) uint8_t clr(void)
{
    cc.c = CC_FLAG_CLR;
    cc.v = CC_FLAG_CLR;
    cc.z = CC_FLAG_SET;
    cc.n = CC_FLAG_CLR;

    return 0;
}

/*------------------------------------------------
 * cmp()
 *
 *  Compare 'arg' to 'byte' by subtracting
 *  the byte from the argument and updating the
 *  flags.
 *
 */
inline __attribute__((always_inline))  void cmp(uint8_t arg, uint8_t byte)
{
    uint16_t result;

    result = arg - byte;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(arg, ~byte, result);
}

/*------------------------------------------------
 * cmp16()
 *
 *  Compare 'arg' to 'word' by subtracting
 *  the a 16-bit value from the argument and updating the
 *  flags.
 *
 */
inline __attribute__((always_inline)) void cmp16(uint16_t arg, uint16_t word)
{
    uint32_t result;

    result = arg - word;

    eval_cc_c16(result);
    eval_cc_z16(result);
    eval_cc_v16(arg, ~word, result);
    eval_cc_n16(result);
}

/*------------------------------------------------
 * com()
 *
 *  Complement (bit-wise NOT) a byte.
 *
 *  ~byte
 */
inline __attribute__((always_inline)) uint8_t com(uint8_t byte)
{
    uint8_t result;

    result = ~byte;

    cc.c = CC_FLAG_SET;
    cc.v = CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);

    return result;
}

/*------------------------------------------------
 * wai()
 *
 *  All registers are pushed onto the stack. The CPU then halts 
 *  execution and waits for an unmasked interrupt to occur.
 *
 */
void wai(void)
{
    mem_write(cpu.sp, cpu.pc & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, (cpu.pc >> 8) & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, cpu.x & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, (cpu.x >> 8) & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, cpu.ab.ab.a);
    cpu.sp--;
    mem_write(cpu.sp, cpu.ab.ab.b);
    cpu.sp--;
    mem_write(cpu.sp, get_cc());
    cpu.sp--;
    
    cpu.cpu_state = CPU_HALTED;
}

/*------------------------------------------------
 * daa()
 *
 *  Decimal adjust accumulator A
 *
 */
inline __attribute__((always_inline)) void daa(void)
{
    uint16_t    temp;
    uint16_t    high_nibble;
    uint16_t    low_nibble;

    temp = cpu.ab.ab.a;
    high_nibble = temp & 0xf0;
    low_nibble = temp & 0x0f;

    if ( low_nibble > 0x09 || cc.h )
        temp += 0x06;

    if ( high_nibble > 0x80 && low_nibble > 0x09 )
        temp += 0x60;
    else if (high_nibble > 0x90 || cc.c)
        temp += 0x60;

    cpu.ab.ab.a = temp;

    eval_cc_c(temp);
    eval_cc_z(temp);
    eval_cc_n(temp);
    cc.v = CC_FLAG_CLR;
}

/*------------------------------------------------
 * dec()
 *
 *  Decrement the operand.
 *
 *  byte = byte - 1
 */
inline __attribute__((always_inline)) uint8_t dec(uint8_t byte)
{
    uint16_t result;

    result = byte - 1;

    eval_cc_v(byte, 0xfe, result);
    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * eor()
 *
 *  Exclusive OR accumulator with operand.
 *
 *  acc ^ byte
 */
inline __attribute__((always_inline)) uint8_t eor(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = acc ^ byte;

    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * inc()
 *
 *  Increment the operand.
 *
 *  byte = byte + 1
 */
inline __attribute__((always_inline)) uint8_t inc(uint8_t byte)
{
    uint16_t result;

    result = byte + 1;

    eval_cc_v(byte, 1, result);
    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * lsr()
 *
 *  Logic shift right.
 *
 *  'byte' shift right, MSB replicated with zero, LSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t lsr(uint8_t byte)
{
    uint8_t result;

    cc.c = byte & 0x01 ? CC_FLAG_SET : CC_FLAG_CLR;
    result = (byte >> 1) & 0x7f;
    cc.z = (result) ? CC_FLAG_CLR : CC_FLAG_SET;
    cc.n = CC_FLAG_CLR;
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * lsr16()
 *
 *  Logic shift right 16-bits.
 */
inline __attribute__((always_inline)) uint16_t lsr16(uint16_t word)
{
    uint16_t result;

    cc.c = word & 0x0001 ? CC_FLAG_SET : CC_FLAG_CLR;
    result = (word >> 1) & 0x7fff;
    cc.z = (result) ? CC_FLAG_CLR : CC_FLAG_SET;
    cc.n = CC_FLAG_CLR;
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * lsl()
 *
 *  Logic shift left.
 *
 *  'byte' shift left, LSB replicated with zero, MSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t lsl(uint8_t byte)
{
    uint8_t result;

    cc.c = (byte & 0x80) ? CC_FLAG_SET: CC_FLAG_CLR;
    result = (byte << 1) & 0xfe;
    cc.z = (result) ? CC_FLAG_CLR : CC_FLAG_SET;
    cc.n = (result & 0x80) ? CC_FLAG_SET: CC_FLAG_CLR;
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * lsl16()
 *
 *  Logic shift left 16-bits.
 */
inline __attribute__((always_inline)) uint16_t lsl16(uint16_t word)
{
    uint16_t result;
    
    cc.c = (word & 0x8000) ? CC_FLAG_SET: CC_FLAG_CLR;
    result = (word << 1) & 0xfffe;
    cc.z = (result) ? CC_FLAG_CLR : CC_FLAG_SET;
    cc.n = (result & 0x8000) ? CC_FLAG_SET: CC_FLAG_CLR;
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;
    
    return result;
}



/*------------------------------------------------
 * neg()
 *
 *  Negate byte.
 *  Two's complement: ~byte+1
 *
 */
inline __attribute__((always_inline)) uint8_t neg(uint8_t byte)
{
    uint16_t result;

    result =  0 - byte;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(0, ~byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * ngc()
 *
 *  Negate byte with carry
 *
 */
inline __attribute__((always_inline)) uint8_t ngc(uint8_t byte)
{
    uint16_t result;

    result =  0 - (byte + cc.c);
    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(0, ~byte, result);

    return (uint8_t) result;
}


/*------------------------------------------------
 * or()
 *
 *  Bit-wise logical OR between 'acc' and 'byte'
 *
 */
inline __attribute__((always_inline)) uint8_t or(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = acc | byte;

    cc.v = CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);

    return result;
}

/*------------------------------------------------
 * orcc()
 *
 *  Logical OR condition-code register with operand.
 *
 */
inline __attribute__((always_inline)) void orcc(uint8_t byte)
{
    uint8_t temp_cc;

    temp_cc = get_cc();
    temp_cc |= byte;
    set_cc(temp_cc);
}


/*------------------------------------------------
 * rol()
 *
 *  Rotate left through Carry
 *
 */
inline __attribute__((always_inline)) uint8_t rol(uint8_t byte)
{
    uint16_t    result;

    result = (byte << 1);

    if ( cc.c )
        result |= 0x0001;
    else
        result &= 0xfffe;

    eval_cc_c(result);
    eval_cc_v(byte, byte, result);
    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * ror()
 *
 *  Rotate right through Carry
 *
 */
inline __attribute__((always_inline)) uint8_t ror(uint8_t byte)
{
    uint16_t    result;


    result = byte;

    if ( cc.c )
        result |= 0x0100;
    else
        result &= 0xfeff;

    if ( byte & 0x01 )
        cc.c = CC_FLAG_SET;
    else
        cc.c = CC_FLAG_CLR;

    result = (result >> 1);

    eval_cc_z(result);
    eval_cc_n(result);
    
    cc.v = (cc.n ^ cc.c) ? CC_FLAG_SET: CC_FLAG_CLR;

    return (uint8_t) result;
}

/*------------------------------------------------
 * sbc()
 *
 *  Subtract with carry.
 *
 *  acc-byte-carry
 */
inline __attribute__((always_inline)) uint8_t sbc(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = acc - byte - cc.c;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, ~byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * sub()
 *
 *  Subtract byte from Acc and set flags
 *
 */
inline __attribute__((always_inline)) uint8_t sub(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = acc - byte;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, ~byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * subd()
 *
 *  Subtract word from D accumulator and set flags
 *  Using 2's complement addition.
 *
 */
inline __attribute__((always_inline)) void subd(uint16_t word)
{
    uint16_t acc;
    uint32_t result;

    acc = (cpu.ab.ab.a << 8) + cpu.ab.ab.b;
    result = acc - word;

    cpu.ab.ab.a = (result >> 8) & 0xff;
    cpu.ab.ab.b = result & 0xff;

    eval_cc_c16(result);
    eval_cc_z16(result);
    eval_cc_v16(acc, ~word, result);
    eval_cc_n16(result);
}


/*------------------------------------------------
 * rti()
 *
 *  Return from interrupt
 *
 */
void rti(void)
{
    /* Restore CCR
     */
    cpu.sp++;
    set_cc(mem_read(cpu.sp));

    /* Restore registers 
     */
    cpu.sp++;
    cpu.ab.ab.b = mem_read(cpu.sp);
    cpu.sp++;
    cpu.ab.ab.a = mem_read(cpu.sp);
    
    cpu.sp++;
    cpu.x = mem_read(cpu.sp) << 8;
    cpu.sp++;
    cpu.x += mem_read(cpu.sp);

    /* Restore PC and return
     */
    cpu.sp++;
    cpu.pc = mem_read(cpu.sp) << 8;
    cpu.sp++;
    cpu.pc += mem_read(cpu.sp);
}


/*------------------------------------------------
 * swi()
 *
 *  Software interrupt.
 */
inline __attribute__((always_inline)) void swi(void)
{
    mem_write(cpu.sp, cpu.pc & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, (cpu.pc >> 8) & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, cpu.x & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, (cpu.x >> 8) & 0xff);
    cpu.sp--;
    mem_write(cpu.sp, cpu.ab.ab.a);
    cpu.sp--;
    mem_write(cpu.sp, cpu.ab.ab.b);
    cpu.sp--;
    mem_write(cpu.sp, get_cc());
    cpu.sp--;    

    cc.i = CC_FLAG_SET;
    cpu.pc = (mem_read(VEC_SWI) << 8) + mem_read(VEC_SWI+1);
}


/*------------------------------------------------
 * tst()
 *
 *  Test 8 bit operand and set V,Z,N,C flags.
 *
 */
void inline __attribute__((always_inline)) tst(uint8_t byte)
{
    eval_cc_z((uint16_t) byte);
    eval_cc_n((uint16_t) byte);
    cc.v = CC_FLAG_CLR;
    cc.c = CC_FLAG_CLR;
}


/*------------------------------------------------
 * get_eff_addr()
 *
 *  Calculate and return effective address.
 *  Resolve addressing mode, calculate effective address.
 *  Modifies 'pc' and appropriate index register.
 *
 *  param:  Command op code and command cycles and bytes count to update if needed.
 *  return: Effective Address, '0' if error
 */
inline __attribute__((always_inline)) int get_eff_addr(int mode)
{
    uint16_t    operand;
    uint16_t    effective_addr = 0;

    switch ( mode )
    {
        case ADDR_DIRECT:
            return mem_read_pc(cpu.pc++);
            break;

        case ADDR_RELATIVE:
            operand = mem_read_pc(cpu.pc++);
            return (cpu.pc + SIG_EXTEND(operand)) & 0xffff;
            break;

        case ADDR_INDEXED:
            operand = mem_read_pc(cpu.pc++);
            effective_addr = cpu.x + operand;
            break;

        case ADDR_EXTENDED:
            effective_addr = (mem_read_pc(cpu.pc++) << 8);
            effective_addr += mem_read_pc(cpu.pc++);
            break;

        case ADDR_IMMEDIATE:
            effective_addr = cpu.pc;
            cpu.pc += 1;
            break;

        case ADDR_LIMMEDIATE:
            effective_addr = cpu.pc;
            cpu.pc += 2;
            break;            

        case ADDR_INHERENT:
            break;

        default:
            break;
    }

    return effective_addr;
}


/*------------------------------------------------
 * eval_cc_c()
 *
 *  Evaluate carry bit of 8-bit input value and set/clear CC.C flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_c(uint16_t value)
{
    cc.c = (value & 0x100) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_c16()
 *
 *  Evaluate carry bit of 16-bit input value and set/clear CC.C flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_c16(uint32_t value)
{
    cc.c = (value & 0x00010000) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_z()
 *
 *  Evaluate zero value of input and set/clear CC.Z flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_z(uint16_t value)
{
    cc.z = !(value & 0x00ff) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_z16()
 *
 *  Evaluate zero value of input and set/clear CC.Z flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_z16(uint32_t value)
{
    cc.z = !(value & 0x0000ffff) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_n()
 *
 *  Evaluate sign bit value of input and set/clear CC.N flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_n(uint16_t value)
{
    cc.n = (value & 0x0080) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_n16()
 *
 *  Evaluate sign bit value of input and set/clear CC.N flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_n16(uint32_t value)
{
    cc.n = (value & 0x00008000) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_v()
 *
 *  Evaluate overflow bit value of input and set/clear CC.V flag.
 *  Use the C(in) != C(out) method, note the C(out) shift to align the bit location
 *  for a bit-wise XOR.
 *  source: http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
 *  Ken Shirriff: https://www.righto.com/2012/12/
 *
 *  param:  Input operands and result
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_v(uint8_t val1, uint8_t val2, uint16_t result)
{
    cc.v = ((val1 ^ result) & (val2 ^ result) & 0x0080) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_v16()
 *
 *  Evaluate overflow bit value of input and set/clear CC.V flag.
 *  Use the C(in) != C(out) method, note the C(out) shift to align the bit location
 *  for a bit-wise XOR.
 *  source: http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
 *  Ken Shirriff: https://www.righto.com/2012/12/
 *
 *  param:  Input operands and result
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_v16(uint16_t val1, uint16_t val2, uint32_t result)
{
    cc.v = ((val1 ^ result) & (val2 ^ result) & 0x00008000) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_h()
 *
 *  Evaluate half carry bit and set/clear CC.H flag.
 *  source: https://retrocomputing.stackexchange.com/questions/11262/can-someone-explain-this-algorithm-used-to-compute-the-auxiliary-carry-flag
 *
 *  param:  Input operands and result
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_h(uint8_t val1, uint8_t val2, uint8_t result)
{
    /* Half carry in 6803 is only relevant/valid for additions ADD and ADC
     */
    cc.h = (((val1 ^ val2) ^ result) & 0x10) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * get_cc()
 *
 *  Return value of CC register as a packed 8-bit value
 *
 *  param:  Nothing
 *  return: 8-bit value of CC register
 */
inline __attribute__((always_inline)) uint8_t get_cc(void)
{
    return (uint8_t) (cc.h << 5) + (cc.i << 4) + (cc.n << 3) + (cc.z << 2) + (cc.v << 1) + cc.c;
}

/*------------------------------------------------
 * set_cc()
 *
 *  Set value of CC register from a packed 8-bit value
 *
 *  param:  8-bit value of CC register
 *  return: Nothing
 */
inline void set_cc(uint8_t value)
{
    cc.c = (value & 0x01) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.v = (value & 0x02) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.z = (value & 0x04) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.n = (value & 0x08) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.i = (value & 0x10) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.h = (value & 0x20) ? CC_FLAG_SET : CC_FLAG_CLR;
}
