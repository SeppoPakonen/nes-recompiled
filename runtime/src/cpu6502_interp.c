/**
 * @file cpu6502_interp.c
 * @brief 6502 CPU interpreter for NES runtime
 *
 * Simplified 6502 interpreter extracted from reference emulator.
 * Provides step-by-step execution for interpreter mode.
 */

#include "nesrt.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

/* 6502 instruction cycle counts */
static const uint8_t opcode_cycles[256] = {
    /* 0x00-0x0F */   7, 6, 0, 8, 0, 3, 5, 5, 3, 2, 2, 0, 0, 4, 6, 6,
    /* 0x10-0x1F */   2, 5, 0, 8, 0, 4, 6, 6, 2, 4, 0, 7, 0, 4, 7, 7,
    /* 0x20-0x2F */   6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 0, 4, 4, 6, 6,
    /* 0x30-0x3F */   2, 5, 0, 8, 0, 4, 6, 6, 2, 4, 0, 7, 0, 4, 7, 7,
    /* 0x40-0x4F */   6, 6, 0, 8, 0, 3, 5, 5, 3, 2, 2, 0, 3, 4, 6, 6,
    /* 0x50-0x5F */   2, 5, 0, 8, 0, 4, 6, 6, 2, 4, 0, 7, 0, 4, 7, 7,
    /* 0x60-0x6F */   6, 6, 0, 8, 0, 3, 5, 5, 4, 2, 2, 0, 5, 4, 6, 6,
    /* 0x70-0x7F */   2, 5, 0, 8, 0, 4, 6, 6, 2, 4, 0, 7, 0, 4, 7, 7,
    /* 0x80-0x8F */   2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 0, 4, 4, 4, 4,
    /* 0x90-0x9F */   2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    /* 0xA0-0xAF */   2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 0, 4, 4, 4, 4,
    /* 0xB0-0xBF */   2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    /* 0xC0-0xCF */   2, 6, 0, 8, 3, 3, 5, 5, 2, 2, 2, 0, 4, 4, 6, 6,
    /* 0xD0-0xDF */   2, 5, 0, 8, 0, 4, 6, 6, 2, 4, 0, 7, 0, 4, 7, 7,
    /* 0xE0-0xEF */   2, 6, 0, 8, 3, 3, 5, 5, 2, 2, 2, 0, 4, 4, 6, 6,
    /* 0xF0-0xFF */   2, 5, 0, 8, 0, 4, 6, 6, 2, 4, 0, 7, 0, 4, 7, 7,
};

/* ============================================================================
 * 6502 CPU State
 * ========================================================================== */

typedef struct {
    uint16_t pc;      /* Program counter */
    uint8_t a;        /* Accumulator */
    uint8_t x;        /* X register */
    uint8_t y;        /* Y register */
    uint8_t sp;       /* Stack pointer (low byte, high byte is always $01) */
    uint8_t flags;    /* Status flags: NV-BDIZC */
    uint8_t halted;   /* CPU halted flag */
} NES6502;

/* Flag bits */
#define FLAG_N 0x80  /* Negative */
#define FLAG_V 0x40  /* Overflow */
#define FLAG_B 0x10  /* Break */
#define FLAG_D 0x08  /* Decimal */
#define FLAG_I 0x04  /* Interrupt disable */
#define FLAG_Z 0x02  /* Zero */
#define FLAG_C 0x01  /* Carry */

/* Global CPU state for interpreter mode */
static NES6502 g_cpu;
static int g_cpu_initialized = 0;

/* ============================================================================
 * Memory Access Helpers
 * ========================================================================== */

static uint8_t cpu_read8(NESContext* ctx, uint16_t addr) {
    return nes_read8(ctx, addr);
}

static void cpu_write8(NESContext* ctx, uint16_t addr, uint8_t val) {
    nes_write8(ctx, addr, val);
}

static uint16_t cpu_read16(NESContext* ctx, uint16_t addr) {
    return cpu_read8(ctx, addr) | (cpu_read8(ctx, addr + 1) << 8);
}

static void cpu_write16(NESContext* ctx, uint16_t addr, uint16_t val) {
    cpu_write8(ctx, addr, val & 0xFF);
    cpu_write8(ctx, addr + 1, val >> 8);
}

/* Stack operations - 6502 stack is always at $0100-$01FF */
static void cpu_push8(NESContext* ctx, uint8_t val) {
    cpu_write8(ctx, 0x0100 | g_cpu.sp, val);
    g_cpu.sp--;
}

static uint8_t cpu_pop8(NESContext* ctx) {
    g_cpu.sp++;
    return cpu_read8(ctx, 0x0100 | g_cpu.sp);
}

static void cpu_push16(NESContext* ctx, uint16_t val) {
    cpu_push8(ctx, val >> 8);
    cpu_push8(ctx, val & 0xFF);
}

static uint16_t cpu_pop16(NESContext* ctx) {
    uint16_t lo = cpu_pop8(ctx);
    uint16_t hi = cpu_pop8(ctx);
    return (hi << 8) | lo;
}

/* ============================================================================
 * Flag Operations
 * ========================================================================== */

static inline void set_flag_n(uint8_t val) {
    g_cpu.flags = (g_cpu.flags & ~FLAG_N) | ((val & 0x80) ? FLAG_N : 0);
}

static inline void set_flag_z(uint8_t val) {
    g_cpu.flags = (g_cpu.flags & ~FLAG_Z) | ((val == 0) ? FLAG_Z : 0);
}

static inline void set_flag_nz(uint8_t val) {
    set_flag_n(val);
    set_flag_z(val);
}

static inline int get_flag_c(void) { return (g_cpu.flags & FLAG_C) ? 1 : 0; }
static inline int get_flag_z(void) { return (g_cpu.flags & FLAG_Z) ? 1 : 0; }
static inline int get_flag_n(void) { return (g_cpu.flags & FLAG_N) ? 1 : 0; }
static inline int get_flag_v(void) { return (g_cpu.flags & FLAG_V) ? 1 : 0; }

static inline void set_flag_c(int set) {
    g_cpu.flags = (g_cpu.flags & ~FLAG_C) | (set ? FLAG_C : 0);
}

static inline void set_flag_v(int set) {
    g_cpu.flags = (g_cpu.flags & ~FLAG_V) | (set ? FLAG_V : 0);
}

/* ============================================================================
 * Addressing Modes
 * ========================================================================== */

static uint16_t get_imm(NESContext* ctx) {
    return g_cpu.pc++;
}

static uint16_t get_zp(NESContext* ctx) {
    return cpu_read8(ctx, g_cpu.pc++);
}

static uint16_t get_zp_x(NESContext* ctx) {
    return (cpu_read8(ctx, g_cpu.pc++) + g_cpu.x) & 0xFF;
}

static uint16_t get_zp_y(NESContext* ctx) {
    return (cpu_read8(ctx, g_cpu.pc++) + g_cpu.y) & 0xFF;
}

static uint16_t get_abs(NESContext* ctx) {
    uint16_t addr = cpu_read16(ctx, g_cpu.pc);
    g_cpu.pc += 2;
    return addr;
}

static uint16_t get_abs_x(NESContext* ctx) {
    uint16_t addr = cpu_read16(ctx, g_cpu.pc);
    g_cpu.pc += 2;
    return addr + g_cpu.x;
}

static uint16_t get_abs_y(NESContext* ctx) {
    uint16_t addr = cpu_read16(ctx, g_cpu.pc);
    g_cpu.pc += 2;
    return addr + g_cpu.y;
}

static uint16_t get_ind_x(NESContext* ctx) {
    uint8_t zp = cpu_read8(ctx, g_cpu.pc++);
    uint16_t ptr = (zp + g_cpu.x) & 0xFF;
    return cpu_read16(ctx, ptr);
}

static uint16_t get_ind_y(NESContext* ctx) {
    uint8_t zp = cpu_read8(ctx, g_cpu.pc++);
    uint16_t ptr = cpu_read16(ctx, zp);
    return ptr + g_cpu.y;
}

/* ============================================================================
 * Instruction Implementations
 * ========================================================================== */

static void instr_lda(NESContext* ctx, uint16_t addr) {
    g_cpu.a = cpu_read8(ctx, addr);
    set_flag_nz(g_cpu.a);
}

static void instr_ldx(NESContext* ctx, uint16_t addr) {
    g_cpu.x = cpu_read8(ctx, addr);
    set_flag_nz(g_cpu.x);
}

static void instr_ldy(NESContext* ctx, uint16_t addr) {
    g_cpu.y = cpu_read8(ctx, addr);
    set_flag_nz(g_cpu.y);
}

static void instr_sta(NESContext* ctx, uint16_t addr) {
    cpu_write8(ctx, addr, g_cpu.a);
}

static void instr_stx(NESContext* ctx, uint16_t addr) {
    cpu_write8(ctx, addr, g_cpu.x);
}

static void instr_sty(NESContext* ctx, uint16_t addr) {
    cpu_write8(ctx, addr, g_cpu.y);
}

static void instr_tax(NESContext* ctx) {
    g_cpu.x = g_cpu.a;
    set_flag_nz(g_cpu.x);
}

static void instr_tay(NESContext* ctx) {
    g_cpu.y = g_cpu.a;
    set_flag_nz(g_cpu.y);
}

static void instr_txa(NESContext* ctx) {
    g_cpu.a = g_cpu.x;
    set_flag_nz(g_cpu.a);
}

static void instr_tya(NESContext* ctx) {
    g_cpu.a = g_cpu.y;
    set_flag_nz(g_cpu.a);
}

static void instr_tsx(NESContext* ctx) {
    g_cpu.x = g_cpu.sp;
    set_flag_nz(g_cpu.x);
}

static void instr_txs(NESContext* ctx) {
    g_cpu.sp = g_cpu.x;  /* 6502: SP = 0x0100 | X */
}

static void instr_inx(NESContext* ctx) {
    g_cpu.x++;
    set_flag_nz(g_cpu.x);
}

static void instr_iny(NESContext* ctx) {
    g_cpu.y++;
    set_flag_nz(g_cpu.y);
}

static void instr_dex(NESContext* ctx) {
    g_cpu.x--;
    set_flag_nz(g_cpu.x);
}

static void instr_dey(NESContext* ctx) {
    g_cpu.y--;
    set_flag_nz(g_cpu.y);
}

static void instr_inc(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr) + 1;
    cpu_write8(ctx, addr, val);
    set_flag_nz(val);
}

static void instr_dec(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr) - 1;
    cpu_write8(ctx, addr, val);
    set_flag_nz(val);
}

static void instr_adc(NESContext* ctx, uint16_t addr) {
    uint8_t m = cpu_read8(ctx, addr);
    uint16_t result = g_cpu.a + m + get_flag_c();
    
    set_flag_nz(result & 0xFF);
    set_flag_v(((g_cpu.a ^ result) & (m ^ result) & 0x80) != 0);
    set_flag_c(result > 0xFF);
    
    g_cpu.a = result & 0xFF;
}

static void instr_sbc(NESContext* ctx, uint16_t addr) {
    uint8_t m = cpu_read8(ctx, addr);
    uint16_t result = g_cpu.a - m - (1 - get_flag_c());
    
    set_flag_nz(result & 0xFF);
    set_flag_v(((g_cpu.a ^ result) & (~m ^ result) & 0x80) != 0);
    set_flag_c(result <= 0xFF);
    
    g_cpu.a = result & 0xFF;
}

static void instr_and(NESContext* ctx, uint16_t addr) {
    g_cpu.a &= cpu_read8(ctx, addr);
    set_flag_nz(g_cpu.a);
}

static void instr_ora(NESContext* ctx, uint16_t addr) {
    g_cpu.a |= cpu_read8(ctx, addr);
    set_flag_nz(g_cpu.a);
}

static void instr_eor(NESContext* ctx, uint16_t addr) {
    g_cpu.a ^= cpu_read8(ctx, addr);
    set_flag_nz(g_cpu.a);
}

static void instr_cmp(NESContext* ctx, uint16_t addr) {
    uint16_t result = g_cpu.a - cpu_read8(ctx, addr);
    set_flag_nz(result & 0xFF);
    set_flag_c(g_cpu.a >= cpu_read8(ctx, addr));
}

static void instr_cpx(NESContext* ctx, uint16_t addr) {
    uint16_t result = g_cpu.x - cpu_read8(ctx, addr);
    set_flag_nz(result & 0xFF);
    set_flag_c(g_cpu.x >= cpu_read8(ctx, addr));
}

static void instr_cpy(NESContext* ctx, uint16_t addr) {
    uint16_t result = g_cpu.y - cpu_read8(ctx, addr);
    set_flag_nz(result & 0xFF);
    set_flag_c(g_cpu.y >= cpu_read8(ctx, addr));
}

static void instr_asl_a(NESContext* ctx) {
    set_flag_c(g_cpu.a & 0x80);
    g_cpu.a <<= 1;
    set_flag_nz(g_cpu.a);
}

static void instr_asl(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr);
    set_flag_c(val & 0x80);
    val <<= 1;
    cpu_write8(ctx, addr, val);
    set_flag_nz(val);
}

static void instr_lsr_a(NESContext* ctx) {
    set_flag_c(g_cpu.a & 0x01);
    g_cpu.a >>= 1;
    set_flag_nz(g_cpu.a);
}

static void instr_lsr(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr);
    set_flag_c(val & 0x01);
    val >>= 1;
    cpu_write8(ctx, addr, val);
    set_flag_nz(val);
}

static void instr_rol_a(NESContext* ctx) {
    int c = get_flag_c();
    set_flag_c(g_cpu.a & 0x80);
    g_cpu.a = (g_cpu.a << 1) | c;
    set_flag_nz(g_cpu.a);
}

static void instr_rol(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr);
    int c = get_flag_c();
    set_flag_c(val & 0x80);
    val = (val << 1) | c;
    cpu_write8(ctx, addr, val);
    set_flag_nz(val);
}

static void instr_ror_a(NESContext* ctx) {
    int c = get_flag_c() ? 0x80 : 0;
    set_flag_c(g_cpu.a & 0x01);
    g_cpu.a = (g_cpu.a >> 1) | c;
    set_flag_nz(g_cpu.a);
}

static void instr_ror(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr);
    int c = get_flag_c() ? 0x80 : 0;
    set_flag_c(val & 0x01);
    val = (val >> 1) | c;
    cpu_write8(ctx, addr, val);
    set_flag_nz(val);
}

static void instr_bit(NESContext* ctx, uint16_t addr) {
    uint8_t val = cpu_read8(ctx, addr);
    set_flag_n(val);
    set_flag_v(val & 0x40);
    set_flag_z(g_cpu.a & val);
}

static void instr_branch(NESContext* ctx, int condition, uint16_t addr) {
    if (condition) {
        int8_t offset = (int8_t)cpu_read8(ctx, addr);
        g_cpu.pc = addr + 1 + offset;
    } else {
        g_cpu.pc = addr + 1;
    }
}

static void instr_jmp_abs(NESContext* ctx, uint16_t addr) {
    g_cpu.pc = addr;
}

static void instr_jmp_ind(NESContext* ctx, uint16_t addr) {
    /* 6502 JMP indirect bug: if address ends in $FF, read high byte from $xx00 */
    if ((addr & 0x00FF) == 0x00FF) {
        uint16_t lo = cpu_read16(ctx, addr & 0xFF00);
        uint8_t hi = cpu_read8(ctx, addr & 0xFF00);
        g_cpu.pc = (hi << 8) | (lo & 0xFF);
    } else {
        g_cpu.pc = cpu_read16(ctx, addr);
    }
}

static void instr_jsr(NESContext* ctx, uint16_t addr) {
    cpu_push16(ctx, g_cpu.pc - 1);  /* Push return address - 1 */
    g_cpu.pc = addr;
}

static void instr_rts(NESContext* ctx) {
    g_cpu.pc = cpu_pop16(ctx) + 1;
}

static void instr_rti(NESContext* ctx) {
    uint8_t flags = cpu_pop8(ctx);
    uint16_t pc = cpu_pop16(ctx);
    g_cpu.flags = (flags & 0xEF) | 0x20;  /* Keep B flag clear, set unused bit */
    g_cpu.pc = pc;
}

static void instr_pha(NESContext* ctx) {
    cpu_push8(ctx, g_cpu.a);
}

static void instr_php(NESContext* ctx) {
    cpu_push8(ctx, g_cpu.flags | FLAG_B);
}

static void instr_pla(NESContext* ctx) {
    g_cpu.a = cpu_pop8(ctx);
    set_flag_nz(g_cpu.a);
}

static void instr_plp(NESContext* ctx) {
    g_cpu.flags = (cpu_pop8(ctx) & 0xEF) | 0x20;
}

static void instr_sec(NESContext* ctx) { set_flag_c(1); }
static void instr_cld(NESContext* ctx) { g_cpu.flags &= ~FLAG_D; }
static void instr_cli(NESContext* ctx) { g_cpu.flags &= ~FLAG_I; }
static void instr_clc(NESContext* ctx) { set_flag_c(0); }
static void instr_sed(NESContext* ctx) { g_cpu.flags |= FLAG_D; }
static void instr_sei(NESContext* ctx) { g_cpu.flags |= FLAG_I; }
static void instr_clv(NESContext* ctx) { g_cpu.flags &= ~FLAG_V; }

static void instr_nop(NESContext* ctx) { (void)ctx; }

static void instr_brk(NESContext* ctx) {
    cpu_push16(ctx, g_cpu.pc);
    cpu_push8(ctx, g_cpu.flags | FLAG_B);
    g_cpu.flags |= FLAG_I;
    g_cpu.pc = cpu_read16(ctx, 0xFFFE);  /* IRQ vector */
}

/* ============================================================================
 * Main Interpreter Loop
 * ========================================================================== */

/* Execute one 6502 instruction */
void nes6502_step(NESContext* ctx) {
    if (!g_cpu_initialized) {
        fprintf(stderr, "[6502] CPU not initialized!\n");
        return;
    }
    
    if (g_cpu.halted) {
        return;
    }
    
    uint8_t opcode = cpu_read8(ctx, g_cpu.pc++);
    
    switch (opcode) {
        /* LDA */
        case 0xA9: instr_lda(ctx, get_imm(ctx)); break;
        case 0xA5: instr_lda(ctx, get_zp(ctx)); break;
        case 0xB5: instr_lda(ctx, get_zp_x(ctx)); break;
        case 0xAD: instr_lda(ctx, get_abs(ctx)); break;
        case 0xBD: instr_lda(ctx, get_abs_x(ctx)); break;
        case 0xB9: instr_lda(ctx, get_abs_y(ctx)); break;
        case 0xA1: instr_lda(ctx, get_ind_x(ctx)); break;
        case 0xB1: instr_lda(ctx, get_ind_y(ctx)); break;
        
        /* LDX */
        case 0xA2: instr_ldx(ctx, get_imm(ctx)); break;
        case 0xA6: instr_ldx(ctx, get_zp(ctx)); break;
        case 0xB6: instr_ldx(ctx, get_zp_y(ctx)); break;
        case 0xAE: instr_ldx(ctx, get_abs(ctx)); break;
        case 0xBE: instr_ldx(ctx, get_abs_y(ctx)); break;
        
        /* LDY */
        case 0xA0: instr_ldy(ctx, get_imm(ctx)); break;
        case 0xA4: instr_ldy(ctx, get_zp(ctx)); break;
        case 0xB4: instr_ldy(ctx, get_zp_x(ctx)); break;
        case 0xAC: instr_ldy(ctx, get_abs(ctx)); break;
        case 0xBC: instr_ldy(ctx, get_abs_x(ctx)); break;
        
        /* STA */
        case 0x85: instr_sta(ctx, get_zp(ctx)); break;
        case 0x95: instr_sta(ctx, get_zp_x(ctx)); break;
        case 0x8D: instr_sta(ctx, get_abs(ctx)); break;
        case 0x9D: instr_sta(ctx, get_abs_x(ctx)); break;
        case 0x99: instr_sta(ctx, get_abs_y(ctx)); break;
        case 0x81: instr_sta(ctx, get_ind_x(ctx)); break;
        case 0x91: instr_sta(ctx, get_ind_y(ctx)); break;
        
        /* STX */
        case 0x86: instr_stx(ctx, get_zp(ctx)); break;
        case 0x96: instr_stx(ctx, get_zp_y(ctx)); break;
        case 0x8E: instr_stx(ctx, get_abs(ctx)); break;
        
        /* STY */
        case 0x84: instr_sty(ctx, get_zp(ctx)); break;
        case 0x94: instr_sty(ctx, get_zp_x(ctx)); break;
        case 0x8C: instr_sty(ctx, get_abs(ctx)); break;
        
        /* Transfers */
        case 0xAA: instr_tax(ctx); break;
        case 0xA8: instr_tay(ctx); break;
        case 0x8A: instr_txa(ctx); break;
        case 0x98: instr_tya(ctx); break;
        case 0xBA: instr_tsx(ctx); break;
        case 0x9A: instr_txs(ctx); break;
        
        /* Increments/Decrements */
        case 0xE8: instr_inx(ctx); break;
        case 0xC8: instr_iny(ctx); break;
        case 0xCA: instr_dex(ctx); break;
        case 0x88: instr_dey(ctx); break;
        case 0xEE: instr_inc(ctx, get_abs(ctx)); break;
        case 0xF6: instr_inc(ctx, get_zp_x(ctx)); break;
        case 0xCE: instr_inc(ctx, get_abs(ctx)); break;
        case 0xDE: instr_dec(ctx, get_abs_x(ctx)); break;
        case 0xD6: instr_dec(ctx, get_zp_x(ctx)); break;
        
        /* Arithmetic */
        case 0x69: instr_adc(ctx, get_imm(ctx)); break;
        case 0x65: instr_adc(ctx, get_zp(ctx)); break;
        case 0x75: instr_adc(ctx, get_zp_x(ctx)); break;
        case 0x6D: instr_adc(ctx, get_abs(ctx)); break;
        case 0x7D: instr_adc(ctx, get_abs_x(ctx)); break;
        case 0x79: instr_adc(ctx, get_abs_y(ctx)); break;
        case 0x61: instr_adc(ctx, get_ind_x(ctx)); break;
        case 0x71: instr_adc(ctx, get_ind_y(ctx)); break;
        
        case 0xE9: instr_sbc(ctx, get_imm(ctx)); break;
        case 0xEB: instr_sbc(ctx, get_imm(ctx)); break;  /* Alternate opcode */
        case 0xE5: instr_sbc(ctx, get_zp(ctx)); break;
        case 0xF5: instr_sbc(ctx, get_zp_x(ctx)); break;
        case 0xED: instr_sbc(ctx, get_abs(ctx)); break;
        case 0xFD: instr_sbc(ctx, get_abs_x(ctx)); break;
        case 0xF9: instr_sbc(ctx, get_abs_y(ctx)); break;
        case 0xE1: instr_sbc(ctx, get_ind_x(ctx)); break;
        case 0xF1: instr_sbc(ctx, get_ind_y(ctx)); break;
        
        /* Logical */
        case 0x29: instr_and(ctx, get_imm(ctx)); break;
        case 0x25: instr_and(ctx, get_zp(ctx)); break;
        case 0x35: instr_and(ctx, get_zp_x(ctx)); break;
        case 0x2D: instr_and(ctx, get_abs(ctx)); break;
        case 0x3D: instr_and(ctx, get_abs_x(ctx)); break;
        case 0x39: instr_and(ctx, get_abs_y(ctx)); break;
        case 0x21: instr_and(ctx, get_ind_x(ctx)); break;
        case 0x31: instr_and(ctx, get_ind_y(ctx)); break;
        
        case 0x09: instr_ora(ctx, get_imm(ctx)); break;
        case 0x05: instr_ora(ctx, get_zp(ctx)); break;
        case 0x15: instr_ora(ctx, get_zp_x(ctx)); break;
        case 0x0D: instr_ora(ctx, get_abs(ctx)); break;
        case 0x1D: instr_ora(ctx, get_abs_x(ctx)); break;
        case 0x19: instr_ora(ctx, get_abs_y(ctx)); break;
        case 0x01: instr_ora(ctx, get_ind_x(ctx)); break;
        case 0x11: instr_ora(ctx, get_ind_y(ctx)); break;
        
        case 0x49: instr_eor(ctx, get_imm(ctx)); break;
        case 0x45: instr_eor(ctx, get_zp(ctx)); break;
        case 0x55: instr_eor(ctx, get_zp_x(ctx)); break;
        case 0x4D: instr_eor(ctx, get_abs(ctx)); break;
        case 0x5D: instr_eor(ctx, get_abs_x(ctx)); break;
        case 0x59: instr_eor(ctx, get_abs_y(ctx)); break;
        case 0x41: instr_eor(ctx, get_ind_x(ctx)); break;
        case 0x51: instr_eor(ctx, get_ind_y(ctx)); break;
        
        /* Compare */
        case 0xC9: instr_cmp(ctx, get_imm(ctx)); break;
        case 0xC5: instr_cmp(ctx, get_zp(ctx)); break;
        case 0xD5: instr_cmp(ctx, get_zp_x(ctx)); break;
        case 0xCD: instr_cmp(ctx, get_abs(ctx)); break;
        case 0xDD: instr_cmp(ctx, get_abs_x(ctx)); break;
        case 0xD9: instr_cmp(ctx, get_abs_y(ctx)); break;
        case 0xC1: instr_cmp(ctx, get_ind_x(ctx)); break;
        case 0xD1: instr_cmp(ctx, get_ind_y(ctx)); break;
        
        case 0xE0: instr_cpx(ctx, get_imm(ctx)); break;
        case 0xE4: instr_cpx(ctx, get_zp(ctx)); break;
        case 0xEC: instr_cpx(ctx, get_abs(ctx)); break;
        
        case 0xC0: instr_cpy(ctx, get_imm(ctx)); break;
        case 0xC4: instr_cpy(ctx, get_zp(ctx)); break;
        case 0xCC: instr_cpy(ctx, get_abs(ctx)); break;
        
        /* Shifts/Rotates */
        case 0x0A: instr_asl_a(ctx); break;
        case 0x06: instr_asl(ctx, get_zp(ctx)); break;
        case 0x16: instr_asl(ctx, get_zp_x(ctx)); break;
        case 0x0E: instr_asl(ctx, get_abs(ctx)); break;
        case 0x1E: instr_asl(ctx, get_abs_x(ctx)); break;
        
        case 0x4A: instr_lsr_a(ctx); break;
        case 0x46: instr_lsr(ctx, get_zp(ctx)); break;
        case 0x56: instr_lsr(ctx, get_zp_x(ctx)); break;
        case 0x4E: instr_lsr(ctx, get_abs(ctx)); break;
        case 0x5E: instr_lsr(ctx, get_abs_x(ctx)); break;
        
        case 0x2A: instr_rol_a(ctx); break;
        case 0x26: instr_rol(ctx, get_zp(ctx)); break;
        case 0x36: instr_rol(ctx, get_zp_x(ctx)); break;
        case 0x2E: instr_rol(ctx, get_abs(ctx)); break;
        case 0x3E: instr_rol(ctx, get_abs_x(ctx)); break;
        
        case 0x6A: instr_ror_a(ctx); break;
        case 0x66: instr_ror(ctx, get_zp(ctx)); break;
        case 0x76: instr_ror(ctx, get_zp_x(ctx)); break;
        case 0x6E: instr_ror(ctx, get_abs(ctx)); break;
        case 0x7E: instr_ror(ctx, get_abs_x(ctx)); break;
        
        /* BIT */
        case 0x24: instr_bit(ctx, get_zp(ctx)); break;
        case 0x2C: instr_bit(ctx, get_abs(ctx)); break;
        
        /* Branches */
        case 0x10: instr_branch(ctx, !get_flag_n(), g_cpu.pc); break;  /* BPL */
        case 0x30: instr_branch(ctx, get_flag_n(), g_cpu.pc); break;   /* BMI */
        case 0x50: instr_branch(ctx, !get_flag_v(), g_cpu.pc); break;  /* BVC */
        case 0x70: instr_branch(ctx, get_flag_v(), g_cpu.pc); break;   /* BVS */
        case 0x90: instr_branch(ctx, !get_flag_c(), g_cpu.pc); break;  /* BCC */
        case 0xB0: instr_branch(ctx, get_flag_c(), g_cpu.pc); break;   /* BCS */
        case 0xD0: instr_branch(ctx, !get_flag_z(), g_cpu.pc); break;  /* BNE */
        case 0xF0: instr_branch(ctx, get_flag_z(), g_cpu.pc); break;   /* BEQ */
        
        /* Jumps */
        case 0x4C: instr_jmp_abs(ctx, get_abs(ctx)); break;
        case 0x6C: instr_jmp_ind(ctx, get_abs(ctx)); break;
        
        /* Subroutine */
        case 0x20: instr_jsr(ctx, cpu_read16(ctx, g_cpu.pc)); g_cpu.pc += 2; break;
        case 0x60: instr_rts(ctx); break;
        case 0x40: instr_rti(ctx); break;
        
        /* Stack */
        case 0x48: instr_pha(ctx); break;
        case 0x08: instr_php(ctx); break;
        case 0x68: instr_pla(ctx); break;
        case 0x28: instr_plp(ctx); break;
        
        /* Flags */
        case 0x38: instr_sec(ctx); break;
        case 0xD8: instr_cld(ctx); break;
        case 0x58: instr_cli(ctx); break;
        case 0x18: instr_clc(ctx); break;
        case 0xF8: instr_sed(ctx); break;
        case 0x78: instr_sei(ctx); break;
        case 0xB8: instr_clv(ctx); break;
        
        /* NOP - unofficial opcodes and padding */
        case 0x04:  /* NOP zp */
        case 0x0C:  /* NOP abs */
        case 0x14:  /* NOP zp,x */
        case 0x1A:  /* NOP */
        case 0x34:  /* NOP zp,x */
        case 0x3A:  /* NOP */
        case 0x44:  /* NOP zp */
        case 0x54:  /* NOP zp,x */
        case 0x5A:  /* NOP */
        case 0x64:  /* NOP zp */
        case 0x7A:  /* NOP */
        case 0x80:  /* NOP imm */
        case 0x82:  /* NOP imm */
        case 0x89:  /* NOP imm */
        case 0xC2:  /* NOP imm */
        case 0xD4:  /* NOP zp,x */
        case 0xDA:  /* NOP */
        case 0xE2:  /* NOP imm */
        case 0xEA:  /* NOP */
        case 0xF4:  /* NOP zp,x */
        case 0xFA:  /* NOP */
            instr_nop(ctx); break;
        
        /* BRK */
        case 0x00: instr_brk(ctx); break;
        
        /* Unknown/Unofficial - treat as NOP with warning */
        default:
            fprintf(stderr, "[6502] Unknown opcode 0x%02X at PC=0x%04X (treating as NOP)\n", opcode, g_cpu.pc - 1);
            /* Don't halt - just skip the byte */
            break;
    }
    
    /* Add CPU cycles for PPU ticking */
    if (ctx) {
        uint8_t cycles = opcode_cycles[opcode];
        nes_add_cycles(ctx, cycles);
    }
}

/* Initialize 6502 CPU */
void nes6502_init(void) {
    memset(&g_cpu, 0, sizeof(g_cpu));
    g_cpu.pc = 0;
    g_cpu.sp = 0xFD;  /* Standard reset value */
    g_cpu.flags = 0x24;  /* Reserved bit set, I flag clear */
    g_cpu.halted = 0;
    g_cpu_initialized = 1;
    fprintf(stderr, "[6502] CPU initialized\n");
}

/* Reset 6502 CPU */
void nes6502_reset(NESContext* ctx) {
    g_cpu.pc = cpu_read16(ctx, 0xFFFC);  /* Reset vector */
    g_cpu.sp = 0xFD;
    g_cpu.flags = 0x24;
    g_cpu.a = 0;
    g_cpu.x = 0;
    g_cpu.y = 0;
    g_cpu.halted = 0;
    fprintf(stderr, "[6502] CPU reset, PC=0x%04X\n", g_cpu.pc);
}

/* Set PC */
void nes6502_set_pc(uint16_t pc) {
    g_cpu.pc = pc;
}

/* Get PC */
uint16_t nes6502_get_pc(void) {
    return g_cpu.pc;
}

/* Get registers for debugging */
void nes6502_get_regs(uint8_t* a, uint8_t* x, uint8_t* y, uint8_t* sp, uint8_t* flags) {
    if (a) *a = g_cpu.a;
    if (x) *x = g_cpu.x;
    if (y) *y = g_cpu.y;
    if (sp) *sp = g_cpu.sp;
    if (flags) *flags = g_cpu.flags;
}
