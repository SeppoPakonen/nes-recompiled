/**
 * @file interpreter.c
 * @brief 6502 interpreter mode for NES runtime
 *
 * When interpreter mode is enabled, we use the 6502 interpreter instead of
 * recompiled code. This is useful for debugging and comparison.
 */

#include "nesrt.h"
#include "nesrt_debug.h"
#include "ppu.h"
#include "cpu6502_interp.h"
#include <stdlib.h>
#include <stdio.h>

void nes_interpret(NESContext* ctx, uint16_t addr) {
    static int initialized = 0;
    
    /* Initialize 6502 CPU on first call */
    if (!initialized) {
        nes6502_init();
        initialized = 1;
    }
    
    /* Set PC and execute one instruction */
    nes6502_set_pc(addr);
    
    /* Debug logging */
#ifdef NES_DEBUG_REGS
    static int step_count = 0;
    step_count++;
    if (step_count <= 100) {
        uint8_t a, x, y, sp, flags;
        nes6502_get_regs(&a, &x, &y, &sp, &flags);
        fprintf(stderr, "[6502] Step %d: PC=0x%04X A=%02X X=%02X Y=%02X SP=01%02X F=%02X\n",
                step_count, nes6502_get_pc(), a, x, y, sp, flags);
    }
#endif
    
    /* Execute one 6502 instruction */
    nes6502_step(ctx);
    
    /* Update context PC from CPU */
    ctx->pc = nes6502_get_pc();
}
