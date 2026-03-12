/**
 * @file ppu.c
 * @brief NES PPU (Picture Processing Unit) stub implementation
 * 
 * NOTE: This is a stub implementation for compilation purposes.
 * Full NES PPU implementation will be added in task006.
 */

#include "ppu.h"
#include "nesrt.h"
#include "nesrt_debug.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Default Color Palette (NES RP2C02)
 * ========================================================================== */

/* Standard NES palette (RP2C02) - 64 colors */
static const uint32_t nes_palette[64] = {
    0xFF545454, 0xFF002478, 0xFF0000A8, 0xFF44009C,
    0xFF940084, 0xFFA80020, 0xFFA00000, 0xFF6C2400,
    0xFF444C00, 0xFF007800, 0xFF008C00, 0xFF007468,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF9C9C9C, 0xFF0058FC, 0xFF0038FC, 0xFF6800F8,
    0xFFBC00E0, 0xFFFC0054, 0xFFFC0000, 0xFFCC4400,
    0xFF7C7C00, 0xFF00B800, 0xFF00AC00, 0xFF00AC48,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFCFCFC, 0xFF58B8FC, 0xFF5858FC, 0xFF8C48FC,
    0xFFFC48FC, 0xFFFC488C, 0xFFFC4840, 0xFFE48C28,
    0xFFBCBC00, 0xFF58FC58, 0xFF28E828, 0xFF28E8B8,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFCFCFC, 0xFFA8E8FC, 0xFFA8D8FC, 0xFFBCB8FC,
    0xFFFCB8FC, 0xFFFCB8D8, 0xFFFCB8A8, 0xFFFCC878,
    0xFFFCFCB8, 0xFFB8FCB8, 0xFFA8F8A8, 0xFFA8FCF8,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
};

/* ============================================================================
 * PPU Initialization
 * ========================================================================== */

void ppu_init(NESPPU* ppu) {
    memset(ppu, 0, sizeof(NESPPU));
    ppu_reset(ppu);
    DBG_PPU("NES PPU initialized (stub)");
}

void ppu_reset(NESPPU* ppu) {
    /* Reset PPU registers to power-on state */
    ppu->ctrl = 0x00;
    ppu->mask = 0x00;
    ppu->status = 0x00;
    ppu->oam_addr = 0x00;
    ppu->oam_data = 0x00;
    ppu->scroll = 0x00;
    ppu->vaddr = 0x0000;
    ppu->data = 0x00;
    
    /* Internal state */
    ppu->vaddr_latch = 0;
    ppu->data_latch = 0;
    ppu->oam_sprite_count = 0;
    memset(ppu->oam_sprites, 0, sizeof(ppu->oam_sprites));
    
    /* Clear OAM */
    memset(ppu->oam, 0, sizeof(ppu->oam));
    
    /* Clear palette with default values */
    memset(ppu->palette, 0, sizeof(ppu->palette));
    
    /* Clear VRAM */
    memset(ppu->vram, 0, sizeof(ppu->vram));
    
    /* Clear framebuffer with black */
    for (int i = 0; i < NES_FRAMEBUFFER_SIZE; i++) {
        ppu->rgb_framebuffer[i] = 0xFF000000;
    }
    
    /* Frame state */
    ppu->frame_ready = false;
    ppu->scanline = 0;
    ppu->cycle = 0;
    
    DBG_PPU("NES PPU reset (stub)");
}

/* ============================================================================
 * PPU Stub Functions
 * ========================================================================== */

/**
 * @brief Stub: Render a scanline
 * 
 * NOTE: Full implementation in task006
 */
static void render_scanline_stub(NESPPU* ppu, NESContext* ctx) {
    (void)ctx;
    
    /* Stub: Fill scanline with a test pattern */
    uint8_t color_idx = (ppu->scanline / 16) % 64;
    uint32_t color = nes_palette[color_idx];
    
    for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
        ppu->rgb_framebuffer[ppu->scanline * NES_SCREEN_WIDTH + x] = color;
    }
}

/**
 * @brief Stub: Update PPU status flags
 */
static void update_status(NESPPU* ppu, NESContext* ctx) {
    (void)ctx;
    /* Stub: No-op for now */
    (void)ppu;
}

void ppu_tick(NESPPU* ppu, NESContext* ctx, uint32_t cycles) {
    /* Stub implementation - just advance cycle counter */
    /* Full PPU timing will be implemented in task006 */
    
    for (uint32_t i = 0; i < cycles; i++) {
        ppu->cycle++;
        
        /* NES PPU runs at ~5.37 MHz (21477272 / 4) */
        /* 341 cycles per scanline, 262 scanlines per frame */
        if (ppu->cycle >= 341) {
            ppu->cycle = 0;
            ppu->scanline++;
            
            if (ppu->scanline >= 262) {
                /* Frame complete */
                ppu->scanline = 0;
                ppu->frame_ready = true;
                ctx->frame_done = 1;
                
                /* Request NMI if enabled */
                if (ppu->ctrl & PPUCTRL_VBLANK_INT) {
                    /* NMI will be handled by the CPU */
                    DBG_PPU("VBlank NMI requested (stub)");
                }
            }
            
            /* Render scanline stub during visible scanlines (0-239) */
            if (ppu->scanline < 240) {
                render_scanline_stub(ppu, ctx);
            }
        }
    }
    
    update_status(ppu, ctx);
}

/* ============================================================================
 * Register Access
 * ========================================================================== */

uint8_t ppu_read_register(NESPPU* ppu, NESContext* ctx, uint16_t addr) {
    (void)ctx;
    
    switch (addr & 0x07) {
        case 0x00: /* $2000 - PPUCTRL (write-only) */
            return 0x00;
            
        case 0x01: /* $2001 - PPUMASK (write-only) */
            return 0x00;
            
        case 0x02: /* $2002 - PPUSTATUS */
            {
                uint8_t status = ppu->status;
                /* Reading status clears VBlank flag and vaddr_latch */
                ppu->status &= ~PPUSTATUS_VBLANK;
                ppu->vaddr_latch = 0;
                DBG_PPU("PPUSTATUS read: 0x%02X", status);
                return status;
            }
            
        case 0x03: /* $2003 - OAMADDR (write-only) */
            return 0x00;
            
        case 0x04: /* $2004 - OAMDATA */
            return ppu->oam[ppu->oam_addr];
            
        case 0x05: /* $2005 - PPUSCROLL (write-only) */
            return 0x00;
            
        case 0x06: /* $2006 - PPUADDR (write-only) */
            return 0x00;
            
        case 0x07: /* $2007 - PPUDATA */
            {
                /* VRAM read - returns buffered data */
                uint8_t data = ppu->data_latch;
                
                /* Update latch with actual data */
                if (ppu->vaddr < 0x3F00) {
                    ppu->data_latch = ppu->vram[ppu->vaddr & 0x1FFF];
                } else {
                    ppu->data_latch = ppu->palette[ppu->vaddr & 0x1F];
                }
                
                /* Increment vaddr */
                if (ppu->ctrl & PPUCTRL_INC_ADDR) {
                    ppu->vaddr += 32;
                } else {
                    ppu->vaddr += 1;
                }
                
                return data;
            }
            
        default:
            return 0x00;
    }
}

void ppu_write_register(NESPPU* ppu, NESContext* ctx, uint16_t addr, uint8_t value) {
    (void)ctx;
    
    switch (addr & 0x07) {
        case 0x00: /* $2000 - PPUCTRL */
            DBG_PPU("PPUCTRL write: 0x%02X", value);
            ppu->ctrl = value;
            break;
            
        case 0x01: /* $2001 - PPUMASK */
            DBG_PPU("PPUMASK write: 0x%02X", value);
            ppu->mask = value;
            break;
            
        case 0x02: /* $2002 - PPUSTATUS (read-only) */
            break;
            
        case 0x03: /* $2003 - OAMADDR */
            ppu->oam_addr = value;
            break;
            
        case 0x04: /* $2004 - OAMDATA */
            ppu->oam[ppu->oam_addr] = value;
            ppu->oam_addr++;
            break;
            
        case 0x05: /* $2005 - PPUSCROLL */
            /* Stub: Just store for now */
            if (ppu->vaddr_latch == 0) {
                /* First write: X scroll */
                ppu->vaddr_latch = 1;
            } else {
                /* Second write: Y scroll */
                ppu->vaddr_latch = 0;
            }
            break;
            
        case 0x06: /* $2006 - PPUADDR */
            if (ppu->vaddr_latch == 0) {
                /* First write: High byte */
                ppu->vaddr = (value & 0x3F) << 8;
                ppu->vaddr_latch = 1;
            } else {
                /* Second write: Low byte */
                ppu->vaddr = (ppu->vaddr & 0xFF00) | value;
                ppu->vaddr_latch = 0;
            }
            break;
            
        case 0x07: /* $2007 - PPUDATA */
            if (ppu->vaddr < 0x3F00) {
                ppu->vram[ppu->vaddr & 0x1FFF] = value;
            } else {
                ppu->palette[ppu->vaddr & 0x1F] = value;
            }
            
            /* Increment vaddr */
            if (ppu->ctrl & PPUCTRL_INC_ADDR) {
                ppu->vaddr += 32;
            } else {
                ppu->vaddr += 1;
            }
            break;
    }
}

/* ============================================================================
 * Frame Handling
 * ========================================================================== */

bool ppu_frame_ready(NESPPU* ppu) {
    return ppu->frame_ready;
}

void ppu_clear_frame_ready(NESPPU* ppu) {
    ppu->frame_ready = false;
}

const uint32_t* ppu_get_framebuffer(NESPPU* ppu) {
    return ppu->rgb_framebuffer;
}
