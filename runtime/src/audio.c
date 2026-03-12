/**
 * @file audio.c
 * @brief NES APU (Audio Processing Unit) stub implementation
 * 
 * NOTE: This is a stub implementation for compilation purposes.
 * Full NES APU implementation will be added in task007.
 */

#include "audio.h"
#include "nesrt_debug.h"
#include "audio_stats.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * NES APU Stub State
 * ========================================================================== */

typedef struct {
    /* Stub state - to be expanded in task007 */
    bool enabled;
    uint32_t cycles;
    
    /* Stub audio output */
    int16_t last_left;
    int16_t last_right;
    
    /* Debug state */
    bool debug_enabled;
} NESApuStub;

/* ============================================================================
 * Public Interface
 * ========================================================================== */

void* nes_audio_create(void) {
    NESApuStub* apu = (NESApuStub*)calloc(1, sizeof(NESApuStub));
    if (!apu) return NULL;
    
    apu->enabled = false;
    apu->cycles = 0;
    apu->last_left = 0;
    apu->last_right = 0;
    apu->debug_enabled = false;
    
    DBG_AUDIO("NES APU created (stub)");
    return apu;
}

void nes_audio_destroy(void* apu_ptr) {
    if (apu_ptr) {
        free(apu_ptr);
    }
    DBG_AUDIO("NES APU destroyed (stub)");
}

void nes_audio_reset(void* apu_ptr) {
    NESApuStub* apu = (NESApuStub*)apu_ptr;
    if (!apu) return;
    
    apu->enabled = false;
    apu->cycles = 0;
    apu->last_left = 0;
    apu->last_right = 0;
    
    DBG_AUDIO("NES APU reset (stub)");
}

uint8_t nes_audio_read(NESContext* ctx, uint16_t addr) {
    (void)ctx;
    
    /* Stub: Return open bus value */
    switch (addr) {
        case 0x4015: /* APU Status */
            /* Stub: Return all channels "ready" */
            return 0x0F;
            
        case 0x4016: /* Controller 1 (handled by joypad) */
        case 0x4017: /* Controller 2 (handled by joypad) */
            /* These are handled by the joypad subsystem */
            return 0x00;
            
        default:
            return 0x00;
    }
}

void nes_audio_write(NESContext* ctx, uint16_t addr, uint8_t value) {
    NESApuStub* apu = (NESApuStub*)ctx->apu;
    (void)apu;
    
    /* Stub: Just log the write */
    DBG_AUDIO("NES APU write: addr=0x%04X value=0x%02X (stub)", addr, value);
    
    /* Enable APU on first write */
    if (apu && !apu->enabled) {
        apu->enabled = true;
    }
}

void nes_audio_step(NESContext* ctx, uint32_t cycles) {
    NESApuStub* apu = (NESApuStub*)ctx->apu;
    if (!apu || !apu->enabled) return;
    
    /* Stub: Just accumulate cycles */
    apu->cycles += cycles;
    
    /* NES APU runs at ~1.79 MHz (21477272 / 12) */
    /* Generate samples at 44100 Hz */
    /* Stub: Output silence for now */
}

void nes_audio_div_tick(void* apu, uint16_t old_div, uint16_t new_div) {
    /* Stub: NES APU doesn't use DIV like GB APU */
    (void)apu;
    (void)old_div;
    (void)new_div;
}

void nes_audio_div_reset(void* apu, uint16_t old_div) {
    /* Stub: NES APU doesn't use DIV like GB APU */
    (void)apu;
    (void)old_div;
}

void nes_audio_get_samples(void* apu_ptr, int16_t* left, int16_t* right) {
    NESApuStub* apu = (NESApuStub*)apu_ptr;
    
    if (apu && apu->enabled) {
        /* Stub: Output low-level tone for testing */
        *left = apu->last_left;
        *right = apu->last_right;
    } else {
        *left = 0;
        *right = 0;
    }
}

void nes_audio_set_debug(bool enabled) {
    printf("[AUDIO] NES APU debug %s (stub)\n", enabled ? "enabled" : "disabled");
    (void)enabled;
}

void nes_audio_set_debug_capture_seconds(uint32_t seconds) {
    printf("[AUDIO] NES APU debug capture seconds: %u (stub)\n", seconds);
    (void)seconds;
}

void nes_audio_set_debug_trace(bool enabled) {
    printf("[AUDIO] NES APU debug trace %s (stub)\n", enabled ? "enabled" : "disabled");
    (void)enabled;
}
