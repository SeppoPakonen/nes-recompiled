#include "nesrt.h"
#include "nesrt_debug.h"
#include "ppu.h"
#include <stdlib.h>

/* ============================================================================
 * SM83 Opcode Cycle Timing Tables
 * Based on Pan Docs: https://gbdev.io/pandocs/CPU_Instruction_Set.html
 * Values are in T-cycles (1 M-cycle = 4 T-cycles)
 * For conditional branches, this is the NOT-TAKEN timing; taken branches add extra cycles
 * ========================================================================== */

/* Main opcode cycles (0x00-0xFF) */
static const uint8_t OPCODE_CYCLES[256] = {
    /* 0x00-0x0F */   4, 12,  8,  8,  4,  4,  8,  4, 20,  8,  8,  8,  4,  4,  8,  4,
    /* 0x10-0x1F */   4, 12,  8,  8,  4,  4,  8,  4, 12,  8,  8,  8,  4,  4,  8,  4,
    /* 0x20-0x2F */   8, 12,  8,  8,  4,  4,  8,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    /* 0x30-0x3F */   8, 12,  8,  8, 12, 12, 12,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    /* 0x40-0x4F */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0x50-0x5F */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0x60-0x6F */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0x70-0x7F */   8,  8,  8,  8,  8,  8,  4,  8,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0x80-0x8F */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0x90-0x9F */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0xA0-0xAF */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0xB0-0xBF */   4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 0xC0-0xCF */   8, 12, 12, 16, 12, 16,  8, 16,  8, 16, 12,  4, 12, 24,  8, 16,
    /* 0xD0-0xDF */   8, 12, 12,  4, 12, 16,  8, 16,  8, 16, 12,  4, 12,  4,  8, 16,
    /* 0xE0-0xEF */  12, 12,  8,  4,  4, 16,  8, 16, 16,  4, 16,  4,  4,  4,  8, 16,
    /* 0xF0-0xFF */  12, 12,  8,  4,  4, 16,  8, 16, 12,  8, 16,  4,  4,  4,  8, 16
};

/* CB-prefix opcode cycles - all are 8 cycles except (HL) operations which are 16 */
static const uint8_t CB_OPCODE_CYCLES[256] = {
    /* 0x00-0x0F */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0x10-0x1F */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0x20-0x2F */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0x30-0x3F */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0x40-0x4F */   8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 0x50-0x5F */   8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 0x60-0x6F */   8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 0x70-0x7F */   8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 0x80-0x8F */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0x90-0x9F */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0xA0-0xAF */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0xB0-0xBF */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0xC0-0xCF */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0xD0-0xDF */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0xE0-0xEF */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 0xF0-0xFF */   8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8
};

/* Extra cycles for conditional branches when taken */
#define BRANCH_TAKEN_EXTRA 4   /* JR cc, JP cc add 4 cycles when taken */
#define CALL_TAKEN_EXTRA 12    /* CALL cc adds 12 cycles when taken */
#define RET_TAKEN_EXTRA 12     /* RET cc adds 12 cycles when taken */

/* Helper macros for instruction arguments */
#define READ8(ctx) nes_read8(ctx, ctx->pc++)
#define READ16(ctx) (ctx->pc += 2, nes_read16(ctx, ctx->pc - 2))

static uint8_t get_reg8(NESContext* ctx, uint8_t idx) {
    switch (idx) {
        case 0: return ctx->b;
        case 1: return ctx->c;
        case 2: return ctx->d;
        case 3: return ctx->e;
        case 4: return ctx->h;
        case 5: return ctx->l;
        case 6: return nes_read8(ctx, ctx->hl);
        case 7: return ctx->a;
    }
    return 0;
}

static void set_reg8(NESContext* ctx, uint8_t idx, uint8_t val) {
    switch (idx) {
        case 0: ctx->b = val; break;
        case 1: ctx->c = val; break;
        case 2: ctx->d = val; break;
        case 3: ctx->e = val; break;
        case 4: ctx->h = val; break;
        case 5: ctx->l = val; break;
        case 6: nes_write8(ctx, ctx->hl, val); break;
        case 7: ctx->a = val; break;
    }
}

void nes_interpret(NESContext* ctx, uint16_t addr) {
    /* Set PC to the address we want to execute */
    ctx->pc = addr;
    
    /* Interpreter entry logging */
#ifdef NES_DEBUG_REGS
    static int entry_count = 0;
    entry_count++;
    if (entry_count <= 100) {
        fprintf(stderr, "[INTERP] Enter interpreter at 0x%04X (entry #%d)\n", addr, entry_count);
    }
#endif
    nesrt_log_trace(ctx, (addr < 0x4000) ? 0 : ctx->rom_bank, addr);

    uint32_t instructions_executed = 0;

    while (!ctx->stopped) {
        instructions_executed++;
        
        /* Debug logging */
        (void)instructions_executed; /* Avoid unused warning */
        
        /* Check instruction limit */
        if (nesrt_instruction_limit > 0 && nesrt_instruction_count >= nesrt_instruction_limit) {
            fprintf(stderr, "[LIMIT] Reached instruction limit %llu\n", (unsigned long long)nesrt_instruction_limit);
            exit(0);
        }
        nesrt_instruction_count++;

        /* Debug logging */
        if (nesrt_trace_enabled) {
             DBG_GENERAL("Interpreter (0x%04X): Regs A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X SP=%04X HL=%04X",
                        ctx->pc, ctx->a, ctx->b, ctx->c, ctx->d, ctx->e, ctx->h, ctx->l, ctx->sp, ctx->hl);
        }

#ifdef NES_DEBUG_REGS
        if (1) { /* Always log if REGS is enabled */
            DBG_GENERAL("Interpreter (0x%04X): Regs A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X SP=%04X HL=%04X",
                        ctx->pc, ctx->a, ctx->b, ctx->c, ctx->d, ctx->e, ctx->h, ctx->l, ctx->sp, ctx->hl);
            
            if (ctx->pc >= 0x8000) {
                static uint16_t last_dump_pc = 0;
                if (ctx->pc != last_dump_pc) {
                    DBG_GENERAL("Code at 0x%04X: %02X %02X %02X %02X %02X %02X %02X %02X",
                                ctx->pc, nes_read8(ctx, ctx->pc), nes_read8(ctx, ctx->pc+1), 
                                nes_read8(ctx, ctx->pc+2), nes_read8(ctx, ctx->pc+3),
                                nes_read8(ctx, ctx->pc+4), nes_read8(ctx, ctx->pc+5),
                                nes_read8(ctx, ctx->pc+6), nes_read8(ctx, ctx->pc+7));
                    last_dump_pc = ctx->pc;
                }
            }
        }
#endif
        
        /* NOTE: Previously we returned immediately when pc < 0x8000 (ROM area),
         * expecting the dispatcher to have compiled code for all ROM addresses.
         * However, static analysis may miss code paths (like cpu_instrs.gb's 
         * callback tables), so we MUST interpret ROM code too when called.
         * The interpreter is now a universal fallback for ANY uncompiled code.
         */

        /* HRAM DMA Interception */
        /* Check for standard HRAM DMA routine: LDH (0xFF46), A */
        if (ctx->pc >= 0xFF80 && ctx->pc <= 0xFFFE) {
             // DBG_GENERAL("Interpreter: Executing HRAM at 0x%04X", ctx->pc);
             // if (ctx->pc == 0xFFB6) {
             //     DBG_GENERAL("HRAM[0xFFB6]: %02X %02X %02X %02X %02X %02X %02X %02X", 
             //        nes_read8(ctx, 0xFFB6), nes_read8(ctx, 0xFFB7), 
             //        nes_read8(ctx, 0xFFB8), nes_read8(ctx, 0xFFB9),
             //        nes_read8(ctx, 0xFFBA), nes_read8(ctx, 0xFFBB),
             //        nes_read8(ctx, 0xFFBC), nes_read8(ctx, 0xFFBD));
             // }
             uint8_t op = nes_read8(ctx, ctx->pc);
             if (op == 0xE0 && nes_read8(ctx, ctx->pc + 1) == 0x46) {
                 // DBG_GENERAL("Interpreter: Intercepted HRAM DMA at 0x%04X", ctx->pc);
                 nes_write8(ctx, 0xFF46, ctx->a);
                 nes_ret(ctx); /* Execute RET */
                 return;
             }
        }

        uint8_t opcode;
        if (ctx->halt_bug) {
            /* HALT bug: Read opcode without incrementing PC (PC fails to advance) */
            opcode = nes_read8(ctx, ctx->pc);
            ctx->halt_bug = 0;
        } else {
            opcode = READ8(ctx);
        }
        
        /* Track cycles for this instruction - will be adjusted for branches/CB prefix */
        uint8_t cycles = OPCODE_CYCLES[opcode];
        uint8_t extra_cycles = 0;  /* For conditional branches when taken */
        
        switch (opcode) {
            case 0x00: /* NOP */ break;
            
            case 0x07: nes_rlca(ctx); break;
            case 0x0F: nes_rrca(ctx); break;
            case 0x17: nes_rla(ctx); break;
            case 0x1F: nes_rra(ctx); break;
            case 0x27: nes_daa(ctx); break;
            case 0x2F: ctx->a = ~ctx->a; ctx->f_n = 1; ctx->f_h = 1; break; /* CPL */
            case 0x37: ctx->f_n = 0; ctx->f_h = 0; ctx->f_c = 1; break; /* SCF */
            case 0x3F: ctx->f_n = 0; ctx->f_h = 0; ctx->f_c = !ctx->f_c; break; /* CCF */
            
            case 0x10: nes_stop(ctx); ctx->pc++; break; /* STOP 0 */
            
            /* 8-bit Loads */
            case 0x06: ctx->b = READ8(ctx); break; /* LD B,n */
            case 0x0E: ctx->c = READ8(ctx); break; /* LD C,n */
            case 0x16: ctx->d = READ8(ctx); break; /* LD D,n */
            case 0x1E: ctx->e = READ8(ctx); break; /* LD E,n */
            case 0x26: ctx->h = READ8(ctx); break; /* LD H,n */
            case 0x2E: ctx->l = READ8(ctx); break; /* LD L,n */
            case 0x3E: ctx->a = READ8(ctx); break; /* LD A,n */
            
            /* Complete LD r, r' instructions (0x40-0x7F) */
            /* LD B, r */
            case 0x40: ctx->b = ctx->b; break; /* LD B,B */
            case 0x41: ctx->b = ctx->c; break; /* LD B,C */
            case 0x42: ctx->b = ctx->d; break; /* LD B,D */
            case 0x43: ctx->b = ctx->e; break; /* LD B,E */
            case 0x44: ctx->b = ctx->h; break; /* LD B,H */
            case 0x45: ctx->b = ctx->l; break; /* LD B,L */
            case 0x46: ctx->b = nes_read8(ctx, ctx->hl); break; /* LD B,(HL) */
            case 0x47: ctx->b = ctx->a; break; /* LD B,A */
            
            /* LD C, r */
            case 0x48: ctx->c = ctx->b; break; /* LD C,B */
            case 0x49: ctx->c = ctx->c; break; /* LD C,C */
            case 0x4A: ctx->c = ctx->d; break; /* LD C,D */
            case 0x4B: ctx->c = ctx->e; break; /* LD C,E */
            case 0x4C: ctx->c = ctx->h; break; /* LD C,H */
            case 0x4D: ctx->c = ctx->l; break; /* LD C,L */
            case 0x4E: ctx->c = nes_read8(ctx, ctx->hl); break; /* LD C,(HL) */
            case 0x4F: ctx->c = ctx->a; break; /* LD C,A */
            
            /* LD D, r */
            case 0x50: ctx->d = ctx->b; break; /* LD D,B */
            case 0x51: ctx->d = ctx->c; break; /* LD D,C */
            case 0x52: ctx->d = ctx->d; break; /* LD D,D */
            case 0x53: ctx->d = ctx->e; break; /* LD D,E */
            case 0x54: ctx->d = ctx->h; break; /* LD D,H */
            case 0x55: ctx->d = ctx->l; break; /* LD D,L */
            case 0x56: ctx->d = nes_read8(ctx, ctx->hl); break; /* LD D,(HL) */
            case 0x57: ctx->d = ctx->a; break; /* LD D,A */
            
            /* LD E, r */
            case 0x58: ctx->e = ctx->b; break; /* LD E,B */
            case 0x59: ctx->e = ctx->c; break; /* LD E,C */
            case 0x5A: ctx->e = ctx->d; break; /* LD E,D */
            case 0x5B: ctx->e = ctx->e; break; /* LD E,E */
            case 0x5C: ctx->e = ctx->h; break; /* LD E,H */
            case 0x5D: ctx->e = ctx->l; break; /* LD E,L */
            case 0x5E: ctx->e = nes_read8(ctx, ctx->hl); break; /* LD E,(HL) */
            case 0x5F: ctx->e = ctx->a; break; /* LD E,A */
            
            /* LD H, r */
            case 0x60: ctx->h = ctx->b; break; /* LD H,B */
            case 0x61: ctx->h = ctx->c; break; /* LD H,C */
            case 0x62: ctx->h = ctx->d; break; /* LD H,D */
            case 0x63: ctx->h = ctx->e; break; /* LD H,E */
            case 0x64: ctx->h = ctx->h; break; /* LD H,H */
            case 0x65: ctx->h = ctx->l; break; /* LD H,L */
            case 0x66: ctx->h = nes_read8(ctx, ctx->hl); break; /* LD H,(HL) */
            case 0x67: ctx->h = ctx->a; break; /* LD H,A */
            
            /* LD L, r */
            case 0x68: ctx->l = ctx->b; break; /* LD L,B */
            case 0x69: ctx->l = ctx->c; break; /* LD L,C */
            case 0x6A: ctx->l = ctx->d; break; /* LD L,D */
            case 0x6B: ctx->l = ctx->e; break; /* LD L,E */
            case 0x6C: ctx->l = ctx->h; break; /* LD L,H */
            case 0x6D: ctx->l = ctx->l; break; /* LD L,L */
            case 0x6E: ctx->l = nes_read8(ctx, ctx->hl); break; /* LD L,(HL) */
            case 0x6F: ctx->l = ctx->a; break; /* LD L,A */
            
            /* LD (HL), r */
            case 0x70: nes_write8(ctx, ctx->hl, ctx->b); break; /* LD (HL), B */
            case 0x71: nes_write8(ctx, ctx->hl, ctx->c); break; /* LD (HL), C */
            case 0x72: nes_write8(ctx, ctx->hl, ctx->d); break; /* LD (HL), D */
            case 0x73: nes_write8(ctx, ctx->hl, ctx->e); break; /* LD (HL), E */
            case 0x74: nes_write8(ctx, ctx->hl, ctx->h); break; /* LD (HL), H */
            case 0x75: nes_write8(ctx, ctx->hl, ctx->l); break; /* LD (HL), L */
            case 0x76: /* HALT */
                /* HALT bug: If IME=0 and there's a pending interrupt, PC fails to increment */
                if (!ctx->ime && (nes_read8(ctx, 0xFFFF) & nes_read8(ctx, 0xFF0F) & 0x1F)) {
                    ctx->halt_bug = 1;  /* Next instruction byte read twice */
                } else {
                    nes_halt(ctx);
                }
                nes_tick(ctx, cycles);
                return;
            case 0x77: nes_write8(ctx, ctx->hl, ctx->a); break; /* LD (HL), A */
            
            /* LD A, r */
            case 0x78: ctx->a = ctx->b; break; /* LD A,B */
            case 0x79: ctx->a = ctx->c; break; /* LD A,C */
            case 0x7A: ctx->a = ctx->d; break; /* LD A,D */
            case 0x7B: ctx->a = ctx->e; break; /* LD A,E */
            case 0x7C: ctx->a = ctx->h; break; /* LD A,H */
            case 0x7D: ctx->a = ctx->l; break; /* LD A,L */
            case 0x7E: ctx->a = nes_read8(ctx, ctx->hl); break; /* LD A,(HL) */
            case 0x7F: ctx->a = ctx->a; break; /* LD A,A */
            
            case 0xEA: nes_write8(ctx, READ16(ctx), ctx->a); break; /* LD (nn), A */
            case 0xFA: ctx->a = nes_read8(ctx, READ16(ctx)); break; /* LD A, (nn) */
            
            case 0xE0: nes_write8(ctx, 0xFF00 + READ8(ctx), ctx->a); break; /* LDH (n), A */
            case 0xF0: ctx->a = nes_read8(ctx, 0xFF00 + READ8(ctx)); break; /* LDH A, (n) */
            
            case 0xE2: nes_write8(ctx, 0xFF00 + ctx->c, ctx->a); break; /* LD (C), A */
            case 0xF2: ctx->a = nes_read8(ctx, 0xFF00 + ctx->c); break; /* LD A, (C) */
            
            case 0x0A: ctx->a = nes_read8(ctx, ctx->bc); break; /* LD A, (BC) */
            case 0x1A: ctx->a = nes_read8(ctx, ctx->de); break; /* LD A, (DE) */
            case 0x02: nes_write8(ctx, ctx->bc, ctx->a); break; /* LD (BC), A */
            case 0x12: nes_write8(ctx, ctx->de, ctx->a); break; /* LD (DE), A */

            case 0x22: nes_write8(ctx, ctx->hl++, ctx->a); break; /* LD (HL+), A */
            case 0x2A: ctx->a = nes_read8(ctx, ctx->hl++); break; /* LD A, (HL+) */
            case 0x32: nes_write8(ctx, ctx->hl--, ctx->a); break; /* LD (HL-), A */
            case 0x3A: ctx->a = nes_read8(ctx, ctx->hl--); break; /* LD A, (HL-) */
            case 0x08: { /* LD (nn), SP */
                uint16_t addr = READ16(ctx);
                nes_write16(ctx, addr, ctx->sp);
                break;
            }
            /* 16-bit Loads */
            case 0x01: ctx->bc = READ16(ctx); break; /* LD BC, nn */
            case 0x11: ctx->de = READ16(ctx); break; /* LD DE, nn */
            case 0x21: ctx->hl = READ16(ctx); break; /* LD HL, nn */
            case 0x31: ctx->sp = READ16(ctx); break; /* LD SP, nn */
            case 0xF9: ctx->sp = ctx->hl; break; /* LD SP, HL */
            
            /* Stack */
            case 0xC5: nes_push16(ctx, ctx->bc); break; /* PUSH BC */
            case 0xD5: nes_push16(ctx, ctx->de); break; /* PUSH DE */
            case 0xE5: nes_push16(ctx, ctx->hl); break; /* PUSH HL */
            case 0xF5: nes_pack_flags(ctx); nes_push16(ctx, ctx->af & 0xFFF0); break; /* PUSH AF */
            
            case 0xC1: ctx->bc = nes_pop16(ctx); break; /* POP BC */
            case 0xD1: ctx->de = nes_pop16(ctx); break; /* POP DE */
            case 0xE1: ctx->hl = nes_pop16(ctx); break; /* POP HL */
            case 0xF1: {
                uint16_t af = nes_pop16(ctx);
                ctx->af = af & 0xFFF0; /* Lower 4 bits of F are always 0 */
                nes_unpack_flags(ctx);
                break; 
            }
            
            /* ALU 8-bit */
            case 0x04: ctx->b = nes_inc8(ctx, ctx->b); break; /* INC B */
            case 0x05: ctx->b = nes_dec8(ctx, ctx->b); break; /* DEC B */
            case 0x0C: ctx->c = nes_inc8(ctx, ctx->c); break; /* INC C */
            case 0x0D: ctx->c = nes_dec8(ctx, ctx->c); break; /* DEC C */
            case 0x14: ctx->d = nes_inc8(ctx, ctx->d); break; /* INC D */
            case 0x15: ctx->d = nes_dec8(ctx, ctx->d); break; /* DEC D */
            case 0x1C: ctx->e = nes_inc8(ctx, ctx->e); break; /* INC E */
            case 0x1D: ctx->e = nes_dec8(ctx, ctx->e); break; /* DEC E */
            case 0x24: ctx->h = nes_inc8(ctx, ctx->h); break; /* INC H */
            case 0x25: ctx->h = nes_dec8(ctx, ctx->h); break; /* DEC H */
            case 0x2C: ctx->l = nes_inc8(ctx, ctx->l); break; /* INC L */
            case 0x2D: ctx->l = nes_dec8(ctx, ctx->l); break; /* DEC L */
            case 0x3C: ctx->a = nes_inc8(ctx, ctx->a); break; /* INC A */
            case 0x3D: ctx->a = nes_dec8(ctx, ctx->a); break; /* DEC A */
            case 0x34: nes_write8(ctx, ctx->hl, nes_inc8(ctx, nes_read8(ctx, ctx->hl))); break; /* INC (HL) */
            case 0x35: nes_write8(ctx, ctx->hl, nes_dec8(ctx, nes_read8(ctx, ctx->hl))); break; /* DEC (HL) */
            case 0x36: nes_write8(ctx, ctx->hl, READ8(ctx)); break; /* LD (HL), n */

            case 0x80: nes_add8(ctx, ctx->b); break; /* ADD A, B */
            case 0x81: nes_add8(ctx, ctx->c); break; /* ADD A, C */
            case 0x82: nes_add8(ctx, ctx->d); break; /* ADD A, D */
            case 0x83: nes_add8(ctx, ctx->e); break; /* ADD A, E */
            case 0x84: nes_add8(ctx, ctx->h); break; /* ADD A, H */
            case 0x85: nes_add8(ctx, ctx->l); break; /* ADD A, L */
            case 0x86: nes_add8(ctx, nes_read8(ctx, ctx->hl)); break; /* ADD A, (HL) */
            case 0x87: nes_add8(ctx, ctx->a); break; /* ADD A, A */
            case 0xC6: nes_add8(ctx, READ8(ctx)); break; /* ADD A, n */

            case 0x88: nes_adc8(ctx, ctx->b); break; /* ADC A, B */
            case 0x89: nes_adc8(ctx, ctx->c); break; /* ADC A, C */
            case 0x8A: nes_adc8(ctx, ctx->d); break; /* ADC A, D */
            case 0x8B: nes_adc8(ctx, ctx->e); break; /* ADC A, E */
            case 0x8C: nes_adc8(ctx, ctx->h); break; /* ADC A, H */
            case 0x8D: nes_adc8(ctx, ctx->l); break; /* ADC A, L */
            case 0x8E: nes_adc8(ctx, nes_read8(ctx, ctx->hl)); break; /* ADC A, (HL) */
            case 0x8F: nes_adc8(ctx, ctx->a); break; /* ADC A, A */
            case 0xCE: nes_adc8(ctx, READ8(ctx)); break; /* ADC A, n */

            case 0x90: nes_sub8(ctx, ctx->b); break; /* SUB B */
            case 0x91: nes_sub8(ctx, ctx->c); break; /* SUB C */
            case 0x92: nes_sub8(ctx, ctx->d); break; /* SUB D */
            case 0x93: nes_sub8(ctx, ctx->e); break; /* SUB E */
            case 0x94: nes_sub8(ctx, ctx->h); break; /* SUB H */
            case 0x95: nes_sub8(ctx, ctx->l); break; /* SUB L */
            case 0x96: nes_sub8(ctx, nes_read8(ctx, ctx->hl)); break; /* SUB (HL) */
            case 0x97: nes_sub8(ctx, ctx->a); break; /* SUB A */
            case 0xD6: nes_sub8(ctx, READ8(ctx)); break; /* SUB n */

            case 0x98: nes_sbc8(ctx, ctx->b); break; /* SBC A, B */
            case 0x99: nes_sbc8(ctx, ctx->c); break; /* SBC A, C */
            case 0x9A: nes_sbc8(ctx, ctx->d); break; /* SBC A, D */
            case 0x9B: nes_sbc8(ctx, ctx->e); break; /* SBC A, E */
            case 0x9C: nes_sbc8(ctx, ctx->h); break; /* SBC A, H */
            case 0x9D: nes_sbc8(ctx, ctx->l); break; /* SBC A, L */
            case 0x9E: nes_sbc8(ctx, nes_read8(ctx, ctx->hl)); break; /* SBC A, (HL) */
            case 0x9F: nes_sbc8(ctx, ctx->a); break; /* SBC A, A */
            case 0xDE: nes_sbc8(ctx, READ8(ctx)); break; /* SBC A, n */

            case 0xA0: nes_and8(ctx, ctx->b); break; /* AND B */
            case 0xA1: nes_and8(ctx, ctx->c); break; /* AND C */
            case 0xA2: nes_and8(ctx, ctx->d); break; /* AND D */
            case 0xA3: nes_and8(ctx, ctx->e); break; /* AND E */
            case 0xA4: nes_and8(ctx, ctx->h); break; /* AND H */
            case 0xA5: nes_and8(ctx, ctx->l); break; /* AND L */
            case 0xA6: nes_and8(ctx, nes_read8(ctx, ctx->hl)); break; /* AND (HL) */
            case 0xA7: nes_and8(ctx, ctx->a); break; /* AND A */
            case 0xE6: nes_and8(ctx, READ8(ctx)); break; /* AND n */

            case 0xA8: nes_xor8(ctx, ctx->b); break; /* XOR B */
            case 0xA9: nes_xor8(ctx, ctx->c); break; /* XOR C */
            case 0xAA: nes_xor8(ctx, ctx->d); break; /* XOR D */
            case 0xAB: nes_xor8(ctx, ctx->e); break; /* XOR E */
            case 0xAC: nes_xor8(ctx, ctx->h); break; /* XOR H */
            case 0xAD: nes_xor8(ctx, ctx->l); break; /* XOR L */
            case 0xAE: nes_xor8(ctx, nes_read8(ctx, ctx->hl)); break; /* XOR (HL) */
            case 0xAF: nes_xor8(ctx, ctx->a); break; /* XOR A */
            case 0xEE: nes_xor8(ctx, READ8(ctx)); break; /* XOR n */
            
            case 0xB0: nes_or8(ctx, ctx->b); break; /* OR B */
            case 0xB1: nes_or8(ctx, ctx->c); break; /* OR C */
            case 0xB2: nes_or8(ctx, ctx->d); break; /* OR D */
            case 0xB3: nes_or8(ctx, ctx->e); break; /* OR E */
            case 0xB4: nes_or8(ctx, ctx->h); break; /* OR H */
            case 0xB5: nes_or8(ctx, ctx->l); break; /* OR L */
            case 0xB6: nes_or8(ctx, nes_read8(ctx, ctx->hl)); break; /* OR (HL) */
            case 0xB7: nes_or8(ctx, ctx->a); break; /* OR A */
            case 0xF6: nes_or8(ctx, READ8(ctx)); break; /* OR n */

            case 0xB8: nes_cp8(ctx, ctx->b); break; /* CP B */
            case 0xB9: nes_cp8(ctx, ctx->c); break; /* CP C */
            case 0xBA: nes_cp8(ctx, ctx->d); break; /* CP D */
            case 0xBB: nes_cp8(ctx, ctx->e); break; /* CP E */
            case 0xBC: nes_cp8(ctx, ctx->h); break; /* CP H */
            case 0xBD: nes_cp8(ctx, ctx->l); break; /* CP L */
            case 0xBE: nes_cp8(ctx, nes_read8(ctx, ctx->hl)); break; /* CP (HL) */
            case 0xBF: nes_cp8(ctx, ctx->a); break; /* CP A */
            case 0xFE: nes_cp8(ctx, READ8(ctx)); break; /* CP n */


            
            /* ALU 16-bit */
            case 0x03: ctx->bc++; break; /* INC BC */
            case 0x13: ctx->de++; break; /* INC DE */
            case 0x23: ctx->hl++; break; /* INC HL */
            case 0x33: ctx->sp++; break; /* INC SP */
            
            case 0x0B: ctx->bc--; break; /* DEC BC */
            case 0x1B: ctx->de--; break; /* DEC DE */
            case 0x2B: ctx->hl--; break; /* DEC HL */
            case 0x3B: ctx->sp--; break; /* DEC SP */

            case 0x09: nes_add16(ctx, ctx->bc); break; /* ADD HL, BC */
            case 0x19: nes_add16(ctx, ctx->de); break; /* ADD HL, DE */
            case 0x29: nes_add16(ctx, ctx->hl); break; /* ADD HL, HL */
            case 0x39: nes_add16(ctx, ctx->sp); break; /* ADD HL, SP */
            
            case 0xE8: nes_add_sp(ctx, (int8_t)READ8(ctx)); break; /* ADD SP, n */
            case 0xF8: { /* LD HL, SP+n */
                int8_t offset = (int8_t)READ8(ctx);
                uint32_t result = ctx->sp + offset;
                ctx->f_z = 0;
                ctx->f_n = 0;
                ctx->f_h = ((ctx->sp & 0x0F) + (offset & 0x0F)) > 0x0F;
                ctx->f_c = ((ctx->sp & 0xFF) + (offset & 0xFF)) > 0xFF;
                ctx->hl = (uint16_t)result;
                break;
            }

            /* Control Flow */
            case 0xC3: { /* JP nn */
                uint16_t dest = READ16(ctx);
                nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                ctx->pc = dest; 
                nes_tick(ctx, cycles); 
                return; 
            }
            case 0xE9: { /* JP HL */
                nesrt_log_trace(ctx, (ctx->hl < 0x4000) ? 0 : ctx->rom_bank, ctx->hl);
                ctx->pc = ctx->hl; 
                nes_tick(ctx, cycles); 
                return; 
            }
            
            case 0xC2: { /* JP NZ, nn */
                uint16_t dest = READ16(ctx);
                if (!ctx->f_z) { 
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            case 0xCA: { /* JP Z, nn */
                uint16_t dest = READ16(ctx);
                if (ctx->f_z) { 
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            case 0xD2: { /* JP NC, nn */
                uint16_t dest = READ16(ctx);
                if (!ctx->f_c) { 
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            case 0xDA: { /* JP C, nn */
                uint16_t dest = READ16(ctx);
                if (ctx->f_c) { 
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            
            case 0x18: { /* JR n */
                int8_t off = (int8_t)READ8(ctx);
                uint16_t dest = ctx->pc + off;
                nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                ctx->pc = dest;
                nes_tick(ctx, cycles);
                return;
            }
            case 0x20: { /* JR NZ, n */
                int8_t off = (int8_t)READ8(ctx);
                if (!ctx->f_z) { 
                    uint16_t dest = ctx->pc + off;
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            case 0x28: { /* JR Z, n */
                int8_t off = (int8_t)READ8(ctx);
                if (ctx->f_z) { 
                    uint16_t dest = ctx->pc + off;
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            case 0x30: { /* JR NC, n */
                int8_t off = (int8_t)READ8(ctx);
                if (!ctx->f_c) { 
                    uint16_t dest = ctx->pc + off;
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            case 0x38: { /* JR C, n */
                int8_t off = (int8_t)READ8(ctx);
                if (ctx->f_c) { 
                    uint16_t dest = ctx->pc + off;
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    ctx->pc = dest; 
                    nes_tick(ctx, cycles + BRANCH_TAKEN_EXTRA); 
                    return; 
                }
                break;
            }
            
            case 0xCD: { /* CALL nn */
                uint16_t dest = READ16(ctx);
                nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                nes_push16(ctx, ctx->pc);
                ctx->pc = dest;
                nes_tick(ctx, cycles);
                return;
            }
            case 0xC4: { /* CALL NZ, nn */
                uint16_t dest = READ16(ctx);
                if (!ctx->f_z) {
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    nes_push16(ctx, ctx->pc);
                    ctx->pc = dest;
                    nes_tick(ctx, cycles + CALL_TAKEN_EXTRA);
                    return;
                }
                break;
            }
            case 0xCC: { /* CALL Z, nn */
                uint16_t dest = READ16(ctx);
                if (ctx->f_z) {
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    nes_push16(ctx, ctx->pc);
                    ctx->pc = dest;
                    nes_tick(ctx, cycles + CALL_TAKEN_EXTRA);
                    return;
                }
                break;
            }
            case 0xD4: { /* CALL NC, nn */
                uint16_t dest = READ16(ctx);
                if (!ctx->f_c) {
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    nes_push16(ctx, ctx->pc);
                    ctx->pc = dest;
                    nes_tick(ctx, cycles + CALL_TAKEN_EXTRA);
                    return;
                }
                break;
            }
            case 0xDC: { /* CALL C, nn */
                uint16_t dest = READ16(ctx);
                if (ctx->f_c) {
                    nesrt_log_trace(ctx, (dest < 0x4000) ? 0 : ctx->rom_bank, dest);
                    nes_push16(ctx, ctx->pc);
                    ctx->pc = dest;
                    nes_tick(ctx, cycles + CALL_TAKEN_EXTRA);
                    return;
                }
                break;
            }
            
            case 0xC9: /* RET */
                ctx->pc = nes_pop16(ctx);
                nes_tick(ctx, cycles);
                return;
            case 0xC0: /* RET NZ */
                if (!ctx->f_z) { ctx->pc = nes_pop16(ctx); nes_tick(ctx, cycles + RET_TAKEN_EXTRA); return; }
                break;
            case 0xC8: /* RET Z */
                if (ctx->f_z) { ctx->pc = nes_pop16(ctx); nes_tick(ctx, cycles + RET_TAKEN_EXTRA); return; }
                break;
            case 0xD0: /* RET NC */
                if (!ctx->f_c) { ctx->pc = nes_pop16(ctx); nes_tick(ctx, cycles + RET_TAKEN_EXTRA); return; }
                break;
            case 0xD8: /* RET C */
                if (ctx->f_c) { ctx->pc = nes_pop16(ctx); nes_tick(ctx, cycles + RET_TAKEN_EXTRA); return; }
                break;
            case 0xD9: /* RETI */
                ctx->pc = nes_pop16(ctx);
                ctx->ime = 1; /* RETI enables IME immediately */
                nes_tick(ctx, cycles);
                return;
                
            case 0xC7: nes_rst(ctx, 0x00); nes_tick(ctx, cycles); return;
            case 0xCF: nes_rst(ctx, 0x08); nes_tick(ctx, cycles); return;
            case 0xD7: nes_rst(ctx, 0x10); nes_tick(ctx, cycles); return;
            case 0xDF: nes_rst(ctx, 0x18); nes_tick(ctx, cycles); return;
            case 0xE7: nes_rst(ctx, 0x20); nes_tick(ctx, cycles); return;
            case 0xEF: nes_rst(ctx, 0x28); nes_tick(ctx, cycles); return;
            case 0xF7: nes_rst(ctx, 0x30); nes_tick(ctx, cycles); return;
            case 0xFF: nes_rst(ctx, 0x38); nes_tick(ctx, cycles); return;
                
            case 0xF3: ctx->ime = 0; ctx->ime_pending = 0; break; /* DI - also cancel pending EI */
            case 0xFB: ctx->ime_pending = 1; break; /* EI */
            
            /* Unused / Illegal opcodes (No-ops on some hardware, can reach here in tests) */
            case 0xD3: case 0xDB: case 0xDD: case 0xE3: case 0xE4:
            case 0xEB: case 0xEC: case 0xED: case 0xF4: case 0xFC:
            case 0xFD:
                DBG_GENERAL("Interpreter (0x%04X): Executed unused opcode 0x%02X", ctx->pc - 1, opcode);
                break;

            
            /* CB Prefix */
            case 0xCB: {
                uint8_t cb_op = READ8(ctx);
                cycles = CB_OPCODE_CYCLES[cb_op];  /* Override with CB-specific timing */
                uint8_t r = cb_op & 7;
                uint8_t b = (cb_op >> 3) & 7;
                uint8_t val = get_reg8(ctx, r);
                
                if (cb_op < 0x40) {
                    /* Shifts and Rotates */
                    switch (b) {
                        case 0: val = nes_rlc(ctx, val); break;
                        case 1: val = nes_rrc(ctx, val); break;
                        case 2: val = nes_rl(ctx, val); break;
                        case 3: val = nes_rr(ctx, val); break;
                        case 4: val = nes_sla(ctx, val); break;
                        case 5: val = nes_sra(ctx, val); break;
                        case 6: val = nes_swap(ctx, val); break;
                        case 7: val = nes_srl(ctx, val); break;
                    }
                    set_reg8(ctx, r, val);
                }
                else if (cb_op < 0x80) {
                    /* BIT */
                    nes_bit(ctx, b, val);
                }
                else if (cb_op < 0xC0) {
                    /* RES */
                    val &= ~(1 << b);
                    set_reg8(ctx, r, val);
                }
                else {
                    /* SET */
                    val |= (1 << b);
                    set_reg8(ctx, r, val);
                }
                break;
            }
            
            default:
                DBG_GENERAL("Interpreter (0x%04X): Unimplemented opcode 0x%02X", ctx->pc - 1, opcode);
                /* If we return, we might re-execute garbage or loop. Better to stop/break hard? */
                /* For now, return to dispatch (which might call us again if PC didn't advance?)
                   Actually we advanced PC at READ8. */
                return;
        }
        
        /* Per-instruction cycle counting - uses table lookup + extra for branches taken */
        nes_tick(ctx, cycles + extra_cycles);
    }
}
