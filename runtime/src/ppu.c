/**
 * @file ppu.c
 * @brief NES PPU (Picture Processing Unit) implementation
 *
 * Implements the NES RP2C02 Picture Processing Unit with:
 * - Background rendering from pattern tables + nametables
 * - Sprite rendering (8x8 or 8x16)
 * - Priority (sprite vs background)
 * - VBlank generation at scanline 241
 * - NMI generation when VBlank starts and NMI enabled
 * - Nametable mirroring (horizontal/vertical)
 */

#include "ppu.h"
#include "nesrt.h"
#include "nesrt_debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * NES Color Palette (RP2C02)
 * ========================================================================== */

/* Standard NES palette (RP2C02) - 64 colors in ARGB8888 format */
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
 * Helper Functions - VRAM Access with Mirroring
 * ========================================================================== */

/**
 * @brief Get nametable offset based on mirroring mode
 * @param ppu PPU structure
 * @param addr VRAM address (0x2000-0x2FFF range)
 * @return Offset into vram array (0x2000-0x23FF)
 */
static inline uint16_t get_nametable_offset(NESPPU* ppu, uint16_t addr) {
    /* addr is in range 0x2000-0x2FFF */
    uint16_t offset = addr & 0x03FF;  /* 1KB within nametable */

    /* Nametables are stored at vram[0x2000-0x23FF] */
    /* Mirroring is handled by how we map the address */
    (void)ppu;  /* Currently unused - both modes map to same area */

    return 0x2000 + offset;
}

/**
 * @brief Read from VRAM with proper mirroring
 */
static uint8_t ppu_read_vram(NESPPU* ppu, uint16_t addr) {
    addr &= 0x3FFF;  /* Mirror 0x4000-0xFFFF down to 0x0000-0x3FFF */

    if (addr < 0x3F00) {
        /* VRAM access (pattern tables, nametables, attribute tables) */
        if (addr < 0x2000) {
            /* Pattern tables (CHR ROM/RAM) - 0x0000-0x1FFF (8KB) */
            return ppu->vram[addr & 0x1FFF];
        } else {
            /* Nametables and attribute tables - 0x2000-0x3EFF */
            /* Mirror 0x3000-0x3EFF down to 0x2000-0x2EFF */
            /* Then mirror 0x2800-0x2EFF down to 0x2000-0x27FF */
            uint16_t mirrored = addr & 0x2FFF;
            if (mirrored >= 0x2800) {
                mirrored -= 0x800;  /* Mirror to 0x2000-0x27FF range */
            }
            if (mirrored >= 0x2000) {
                uint16_t offset = get_nametable_offset(ppu, mirrored);
                return ppu->vram[offset];
            }
            return ppu->vram[mirrored & 0x0FFF];
        }
    } else {
        /* Palette RAM - 0x3F00-0x3FFF */
        /* Mirrors: 0x3F04, 0x3F08, 0x3F0C mirror to 0x3F00 */
        /*          0x3F14, 0x3F18, 0x3F1C mirror to 0x3F10 */
        uint16_t palette_addr = addr & 0x1F;
        if (palette_addr == 0x00 || palette_addr == 0x04 || palette_addr == 0x08 || palette_addr == 0x0C) {
            palette_addr = 0x00;
        } else if (palette_addr == 0x10 || palette_addr == 0x14 || palette_addr == 0x18 || palette_addr == 0x1C) {
            palette_addr = 0x10;
        }
        return ppu->palette[palette_addr];
    }
}

/**
 * @brief Write to VRAM with proper mirroring
 */
static void ppu_write_vram(NESPPU* ppu, uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;  /* Mirror 0x4000-0xFFFF down to 0x0000-0x3FFF */

    if (addr < 0x3F00) {
        /* VRAM access (pattern tables, nametables, attribute tables) */
        if (addr < 0x2000) {
            /* Pattern tables (CHR ROM/RAM) - 0x0000-0x1FFF (8KB) */
            ppu->vram[addr & 0x1FFF] = value;
        } else {
            /* Nametables and attribute tables - 0x2000-0x3EFF */
            /* Mirror 0x3000-0x3EFF down to 0x2000-0x2EFF */
            /* Then mirror 0x2800-0x2EFF down to 0x2000-0x27FF */
            uint16_t mirrored = addr & 0x2FFF;
            if (mirrored >= 0x2800) {
                mirrored -= 0x800;  /* Mirror to 0x2000-0x27FF range */
            }
            if (mirrored >= 0x2000) {
                ppu->vram[get_nametable_offset(ppu, mirrored)] = value;
            }
        }
    } else {
        /* Palette RAM - 0x3F00-0x3FFF */
        uint16_t palette_addr = addr & 0x1F;
        /* Handle mirrors */
        if (palette_addr == 0x00 || palette_addr == 0x04 || palette_addr == 0x08 || palette_addr == 0x0C) {
            ppu->palette[0x00] = value & 0x3F;
            ppu->palette[0x04] = value & 0x3F;
            ppu->palette[0x08] = value & 0x3F;
            ppu->palette[0x0C] = value & 0x3F;
            ppu->palette[0x10] = value & 0x3F;
            ppu->palette[0x14] = value & 0x3F;
            ppu->palette[0x18] = value & 0x3F;
            ppu->palette[0x1C] = value & 0x3F;
        } else if (palette_addr == 0x10 || palette_addr == 0x14 || palette_addr == 0x18 || palette_addr == 0x1C) {
            ppu->palette[0x10] = value & 0x3F;
            ppu->palette[0x14] = value & 0x3F;
            ppu->palette[0x18] = value & 0x3F;
            ppu->palette[0x1C] = value & 0x3F;
        } else {
            ppu->palette[palette_addr] = value & 0x3F;
        }
    }
}

/* ============================================================================
 * Rendering Functions
 * ========================================================================== */

/**
 * @brief Get pattern table data for background
 */
static inline uint8_t get_bg_pattern(NESPPU* ppu, uint16_t tile_index, uint8_t fine_y, uint8_t fine_x) {
    uint16_t pattern_addr;
    
    /* Pattern table address: tile_index * 16 + fine_y */
    if (ppu->ctrl & PPUCTRL_BG_TABLE) {
        pattern_addr = 0x1000 + (tile_index * 16) + fine_y;
    } else {
        pattern_addr = 0x0000 + (tile_index * 16) + fine_y;
    }
    
    uint8_t bit0 = ppu_read_vram(ppu, pattern_addr);
    uint8_t bit1 = ppu_read_vram(ppu, pattern_addr + 8);
    
    /* Extract bit at fine_x position (bit 7 is leftmost) */
    uint8_t pixel = ((bit0 >> (7 - fine_x)) & 1) | (((bit1 >> (7 - fine_x)) & 1) << 1);
    return pixel;
}

/**
 * @brief Get pattern table data for sprite
 */
static inline uint8_t get_sprite_pattern(NESPPU* ppu, uint8_t tile_index, uint8_t fine_y, uint8_t fine_x, 
                                          uint8_t attr, uint8_t sprite_height) {
    uint16_t pattern_addr;
    uint8_t y_offset = fine_y;
    
    /* Handle Y flip */
    if (attr & OAM_ATTR_FLIP_Y) {
        y_offset = (sprite_height - 1) - y_offset;
    }
    
    /* Handle 8x16 sprites */
    if (sprite_height == 16) {
        /* In 8x16 mode, tile_index bit 0 selects pattern table */
        uint8_t tile_num = tile_index >> 1;
        pattern_addr = (tile_num * 32) + (y_offset & 7);
        if (tile_index & 1) {
            pattern_addr += 0x1000;  /* Second pattern table */
        }
        /* Handle Y flip for bottom half */
        if ((fine_y & 8) && !(attr & OAM_ATTR_FLIP_Y)) {
            pattern_addr += 8;
        } else if (!(fine_y & 8) && (attr & OAM_ATTR_FLIP_Y)) {
            pattern_addr += 8;
        }
    } else {
        /* 8x8 sprites */
        pattern_addr = (tile_index * 16) + y_offset;
        if (ppu->ctrl & PPUCTRL_SPRITE_TABLE) {
            pattern_addr += 0x1000;
        }
    }
    
    uint8_t bit0 = ppu_read_vram(ppu, pattern_addr);
    uint8_t bit1 = ppu_read_vram(ppu, pattern_addr + 8);
    
    /* Handle X flip */
    uint8_t x_pos = fine_x;
    if (attr & OAM_ATTR_FLIP_X) {
        x_pos = 7 - x_pos;
    }
    
    /* Extract bit at x_pos position */
    uint8_t pixel = ((bit0 >> (7 - x_pos)) & 1) | (((bit1 >> (7 - x_pos)) & 1) << 1);
    return pixel;
}

/**
 * @brief Get attribute byte for background tile
 */
static inline uint8_t get_attribute(NESPPU* ppu, uint16_t vaddr) {
    /* Attribute table address: 0x23C0 + nametable select + coarse Y/8 * 4 + coarse X/8 */
    uint16_t attr_addr = 0x23C0 + (vaddr & 0x0C00) + ((vaddr >> 4) & 0x38) + ((vaddr >> 2) & 0x07);
    uint8_t attr = ppu_read_vram(ppu, attr_addr);
    
    /* Select palette based on coarse X and Y position within 4x4 tile block */
    uint8_t shift = ((vaddr >> 4) & 4) | (vaddr & 2);
    return (attr >> shift) & 0x03;
}

/**
 * @brief Render a single pixel of background
 */
static uint8_t render_background_pixel(NESPPU* ppu, int x, uint8_t* palette_out) {
    /* Check if background is enabled */
    if (!(ppu->mask & PPUMASK_SHOW_BG)) {
        return 0;
    }
    
    /* Check if background is hidden in leftmost 8 pixels */
    if (!(ppu->mask & PPUMASK_SHOW_BG_LEFT) && x < 8) {
        return 0;
    }
    
    /* Calculate fine X from scroll and pixel position */
    uint8_t fine_x = (ppu->fine_x + x) & 7;
    
    /* Calculate coarse scroll position */
    uint16_t vaddr = ppu->temp_vaddr;
    
    /* Get coarse X and Y from vaddr */
    uint8_t coarse_x = vaddr & PPU_VADDR_COARSE_X;
    uint8_t coarse_y = (vaddr & PPU_VADDR_COARSE_Y) >> 5;
    uint8_t fine_y = (vaddr & PPU_VADDR_FINE_Y) >> 12;
    
    /* Calculate tile X position (0-31) */
    uint8_t tile_x = coarse_x + ((ppu->fine_x + x) / 8);
    if (tile_x > 31) {
        /* Handle horizontal nametable wrap */
        tile_x &= 31;
        vaddr ^= PPU_VADDR_HORIZ_BIT;
    }
    
    /* Get tile index from nametable */
    uint16_t nametable_addr = 0x2000 + (coarse_y * 32) + tile_x;
    uint8_t tile_index = ppu_read_vram(ppu, nametable_addr);
    
    /* Get attribute (palette) for this tile */
    uint8_t attr = get_attribute(ppu, vaddr);
    
    /* Get pattern data */
    uint8_t pixel = get_bg_pattern(ppu, tile_index, fine_y, fine_x);
    
    if (pixel == 0) {
        return 0;  /* Transparent */
    }
    
    *palette_out = pixel | (attr << 2);
    return 1;
}

/**
 * @brief Evaluate sprites for current scanline
 */
static void evaluate_sprites(NESPPU* ppu) {
    ppu->sprite_count = 0;
    ppu->sprite_overflow = 0;
    
    uint8_t sprite_height = (ppu->ctrl & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
    
    /* Search through OAM for sprites on this scanline */
    for (int i = 0; i < 64; i++) {
        uint8_t sprite_y = ppu->oam[i * 4];
        
        /* Check if sprite is on this scanline */
        /* Sprite Y=0 means top pixel is at Y=1 (Y=0 is hidden) */
        int diff = ppu->scanline - (sprite_y + 1);
        if (diff >= 0 && diff < sprite_height) {
            if (ppu->sprite_count < 8) {
                ppu->sprite_indices[ppu->sprite_count] = i;
                ppu->sprite_count++;
            } else {
                /* Sprite overflow - more than 8 sprites on scanline */
                ppu->sprite_overflow = 1;
                ppu->status |= PPUSTATUS_SPRITE_OVERFLOW;
                break;
            }
        }
    }
}

/**
 * @brief Render a single pixel of sprite
 */
static uint8_t render_sprite_pixel(NESPPU* ppu, int x, uint8_t* palette_out, uint8_t* priority_out, int* sprite_zero_hit) {
    /* Check if sprites are enabled */
    if (!(ppu->mask & PPUMASK_SHOW_SPR)) {
        return 0;
    }
    
    /* Check if sprites are hidden in leftmost 8 pixels */
    if (!(ppu->mask & PPUMASK_SHOW_SPR_LEFT) && x < 8) {
        return 0;
    }
    
    uint8_t sprite_height = (ppu->ctrl & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
    
    /* Check all sprites on this scanline */
    for (int s = 0; s < ppu->sprite_count; s++) {
        int i = ppu->sprite_indices[s];
        uint8_t sprite_y = ppu->oam[i * 4];
        uint8_t tile_index = ppu->oam[i * 4 + 1];
        uint8_t attr = ppu->oam[i * 4 + 2];
        uint8_t sprite_x = ppu->oam[i * 4 + 3];
        
        /* Calculate X offset within sprite */
        int diff = x - sprite_x;
        if (diff < 0 || diff >= 8) {
            continue;  /* Pixel not in this sprite */
        }
        
        uint8_t fine_y = (ppu->scanline - (sprite_y + 1)) & (sprite_height - 1);
        uint8_t fine_x = diff;
        
        /* Get sprite pattern */
        uint8_t pixel = get_sprite_pattern(ppu, tile_index, fine_y, fine_x, attr, sprite_height);
        
        if (pixel == 0) {
            continue;  /* Transparent */
        }
        
        /* Sprite 0 hit detection */
        if (i == 0 && sprite_zero_hit) {
            *sprite_zero_hit = 1;
        }
        
        *palette_out = 0x10 | ((attr & OAM_ATTR_PALETTE) << 2) | pixel;
        *priority_out = (attr & OAM_ATTR_PRIORITY) ? 1 : 0;
        
        return 1;
    }
    
    return 0;
}

/**
 * @brief Render a scanline to the framebuffer
 */
void ppu_render_scanline(NESPPU* ppu, int scanline) {
    if (scanline < 0 || scanline >= NES_SCREEN_HEIGHT) {
        return;
    }

    /* Debug: Log rendering state */
    DBG_PPU("Render scanline %d: CTRL=0x%02X MASK=0x%02X bg=%d spr=%d",
            scanline, ppu->ctrl, ppu->mask,
            (ppu->mask & PPUMASK_SHOW_BG) ? 1 : 0,
            (ppu->mask & PPUMASK_SHOW_SPR) ? 1 : 0);

    /* TEMPORARY DEBUG: Force-enable rendering if both are disabled */
    /* This is to test the rendering pipeline with ROMs that don't init PPU */
    uint8_t original_mask = ppu->mask;
    if (!(ppu->mask & (PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR))) {
        /* Check if we have CHR data - if so, enable background rendering */
        if (ppu->vram[0] != 0 || ppu->vram[1] != 0) {
            ppu->mask = PPUMASK_SHOW_BG | PPUMASK_SHOW_BG_LEFT;
            DBG_PPU("DEBUG: Forcing background rendering enabled");
        }
    }

    /* Check if rendering is disabled */
    if (!(ppu->mask & (PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR))) {
        /* Fill with gray (background color from palette) */
        uint8_t bg_color_idx = ppu->palette[0] & 0x3F;
        uint32_t gray_color = nes_palette[bg_color_idx];
        for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
            ppu->framebuffer[scanline * NES_SCREEN_WIDTH + x] = gray_color;
        }
        return;
    }

    /* DEBUG: Pixel value counting */
    static int frame_count = 0;
    static int frames_debugged = 0;
    static uint32_t frame_white = 0, frame_black = 0, frame_other = 0;

    uint32_t line_white = 0, line_black = 0, line_other = 0;

    /* Evaluate sprites for this scanline */
    evaluate_sprites(ppu);

    /* Debug: Log first few bytes of CHR data */
    static int chr_logged = 0;
    if (!chr_logged) {
        DBG_VRAM("CHR data[0-15]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 ppu->vram[0], ppu->vram[1], ppu->vram[2], ppu->vram[3],
                 ppu->vram[4], ppu->vram[5], ppu->vram[6], ppu->vram[7],
                 ppu->vram[8], ppu->vram[9], ppu->vram[10], ppu->vram[11],
                 ppu->vram[12], ppu->vram[13], ppu->vram[14], ppu->vram[15]);
        DBG_VRAM("Palette[0-7]: %02X %02X %02X %02X %02X %02X %02X %02X",
                 ppu->palette[0], ppu->palette[1], ppu->palette[2], ppu->palette[3],
                 ppu->palette[4], ppu->palette[5], ppu->palette[6], ppu->palette[7]);
        chr_logged = 1;
    }

    /* Render each pixel */
    int nonzero_pixels = 0;
    for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
        uint8_t bg_palette = 0;
        uint8_t spr_palette = 0;
        uint8_t spr_priority = 0;
        int sprite_zero_hit = 0;

        uint8_t bg_pixel = render_background_pixel(ppu, x, &bg_palette);
        uint8_t spr_pixel = render_sprite_pixel(ppu, x, &spr_palette, &spr_priority, &sprite_zero_hit);

        /* Handle sprite 0 hit */
        if (sprite_zero_hit && bg_pixel && !(ppu->status & PPUSTATUS_SPRITE_ZERO_HIT)) {
            ppu->status |= PPUSTATUS_SPRITE_ZERO_HIT;
        }

        /* Combine background and sprite */
        uint8_t final_palette;
        if (!bg_pixel && spr_pixel) {
            final_palette = spr_palette;
        } else if (bg_pixel && !spr_pixel) {
            final_palette = bg_palette;
        } else if (bg_pixel && spr_pixel) {
            if (spr_priority) {
                final_palette = bg_palette;
            } else {
                final_palette = spr_palette;
            }
        } else {
            final_palette = 0;  /* Background color */
        }

        /* Get color from palette RAM */
        uint32_t color = nes_palette[ppu->palette[final_palette & 0x3F] & 0x3F];
        ppu->framebuffer[scanline * NES_SCREEN_WIDTH + x] = color;

        /* DEBUG: Count pixel colors */
        /* White: all RGB components > 240 (near-white, may be slightly tinted) */
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color >> 0) & 0xFF;
        
        if (r > 240 && g > 240 && b > 240) {
            line_white++;
            frame_white++;
        } else if (r < 15 && g < 15 && b < 15) {
            /* Black: all RGB components < 15 (near-black) */
            line_black++;
            frame_black++;
        } else {
            line_other++;
            frame_other++;
        }

        if (color != nes_palette[ppu->palette[0] & 0x3F]) {
            nonzero_pixels++;
        }
    }

    /* DEBUG: Print statistics every 10 scanlines */
    if (scanline % 10 == 0) {
        printf("[PPU DEBUG] Frame %d Scanline %d: White=%u Black=%u Other=%u (Line: W=%u B=%u O=%u)\n",
               frame_count, scanline, frame_white, frame_black, frame_other,
               line_white, line_black, line_other);
    }

    /* DEBUG: Print frame statistics at end of frame */
    if (scanline == NES_SCREEN_HEIGHT - 1) {
        printf("[PPU DEBUG] Frame %d COMPLETE: White=%u Black=%u Other=%u (%.1f%% black)\n",
               frame_count, frame_white, frame_black, frame_other,
               (frame_black * 100.0f) / (NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT));

        /* Save first frame as PPM */
        if (frames_debugged == 0) {
            FILE* ppm = fopen("debug_frame.ppm", "wb");
            if (ppm) {
                fprintf(ppm, "P6\n%d %d\n255\n", NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT);
                uint8_t* row = (uint8_t*)malloc(NES_SCREEN_WIDTH * 3);
                for (int y = 0; y < NES_SCREEN_HEIGHT; y++) {
                    for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
                        uint32_t p = ppu->framebuffer[y * NES_SCREEN_WIDTH + x];
                        row[x*3+0] = (p >> 16) & 0xFF; /* R */
                        row[x*3+1] = (p >> 8) & 0xFF;  /* G */
                        row[x*3+2] = (p >> 0) & 0xFF;  /* B */
                    }
                    fwrite(row, 1, NES_SCREEN_WIDTH * 3, ppm);
                }
                free(row);
                fclose(ppm);
                printf("[PPU DEBUG] Saved debug_frame.ppm\n");
            } else {
                printf("[PPU DEBUG] Failed to save debug_frame.ppm\n");
            }

            /* Print palette debug info */
            printf("[PPU DEBUG] Palette RAM[0-7]: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                   ppu->palette[0], ppu->palette[1], ppu->palette[2], ppu->palette[3],
                   ppu->palette[4], ppu->palette[5], ppu->palette[6], ppu->palette[7]);
            printf("[PPU DEBUG] Palette RGB values:\n");
            for (int i = 0; i < 8; i++) {
                uint32_t rgb = nes_palette[ppu->palette[i] & 0x3F];
                printf("  [%d] idx=%02X -> RGB=%06X\n", i, ppu->palette[i], rgb & 0xFFFFFF);
            }

            /* Print CHR tile 0 debug */
            printf("[PPU DEBUG] CHR Tile 0 data (first 16 bytes):\n");
            printf("  Bitplane 0: ");
            for (int i = 0; i < 8; i++) {
                printf("%02X ", ppu->vram[i]);
            }
            printf("\n  Bitplane 1: ");
            for (int i = 8; i < 16; i++) {
                printf("%02X ", ppu->vram[i]);
            }
            printf("\n");

            /* Decode tile 0 pixels */
            printf("[PPU DEBUG] Tile 0 pixel data (8x8):\n");
            for (int row = 0; row < 8; row++) {
                uint8_t bit0 = ppu->vram[row];
                uint8_t bit1 = ppu->vram[row + 8];
                printf("  Row %d: ", row);
                for (int col = 0; col < 8; col++) {
                    uint8_t pixel = ((bit0 >> (7 - col)) & 1) | (((bit1 >> (7 - col)) & 1) << 1);
                    printf("%d", pixel);
                }
                printf("\n");
            }
        }

        frames_debugged++;
        frame_count++;
        /* Reset per-frame counters for next frame */
        frame_white = 0;
        frame_black = 0;
        frame_other = 0;
    }

    if (scanline < 10 || scanline % 30 == 0) {
        DBG_PPU("Scanline %d: %d non-bg pixels", scanline, nonzero_pixels);
    }

    /* Restore original mask (was modified for debug forcing) */
    ppu->mask = original_mask;
}

/* ============================================================================
 * PPU Timing and State Machine
 * ========================================================================== */

/**
 * @brief Update VBlank and NMI state
 */
static void update_vblank_nmi(NESPPU* ppu, NESContext* ctx) {
    if ((ppu->status & PPUSTATUS_VBLANK) && (ppu->ctrl & PPUCTRL_VBLANK_INT)) {
        if (!ppu->nmi_requested) {
            ppu->nmi_requested = 1;
            ppu->flags |= PPU_FLAG_NMI_PENDING;
            
            /* Trigger NMI in CPU */
            if (ctx) {
                /* NMI will be processed by the CPU */
                ctx->ime = 1;  /* Enable interrupts */
                /* Set NMI vector - CPU will read from 0xFFFA */
            }
        }
    }
}

/**
 * @brief Process a single PPU cycle
 */
static void ppu_step(NESPPU* ppu, NESContext* ctx) {
    ppu->cycle++;

    if (ppu->cycle >= PPU_CYCLES_PER_SCANLINE) {
        ppu->cycle = 0;
        ppu->scanline++;

        if (ppu->scanline >= PPU_SCANLINES_PER_FRAME) {
            /* Frame complete - wrap to scanline 0 */
            DBG_PPU("FRAME WRAP: scanline=%d -> 0, frame=%d", ppu->scanline, ppu->frame_number + 1);
            ppu->scanline = 0;
            ppu->frame_number++;
            ctx->frame_done = 1;  /* Signal frame completion to main loop */
        }

        /* Handle scanline-specific events */
        if (ppu->scanline == PPU_VBLANK_SCANLINE) {
            /* Start of VBlank */
            ppu->status |= PPUSTATUS_VBLANK;
            ppu->frame_ready = 1;
            DBG_PPU("VBLANK START: scanline=%d, status=0x%02X", ppu->scanline, ppu->status);
            update_vblank_nmi(ppu, ctx);
        } else if (ppu->scanline == PPU_PRE_SCANLINE) {
            /* Pre-render scanline - clear VBlank and Sprite 0 hit */
            DBG_PPU("PRE-RENDER: scanline=%d, clearing VBlank", ppu->scanline);
            ppu->status &= ~(PPUSTATUS_VBLANK | PPUSTATUS_SPRITE_ZERO_HIT);
            ppu->nmi_requested = 0;
            ppu->flags &= ~PPU_FLAG_NMI_PENDING;
        }
    }

    /* Render visible scanlines */
    if (ppu->scanline < PPU_VISIBLE_SCANLINES && ppu->cycle == 1) {
        /* Start rendering this scanline */
        ppu_render_scanline(ppu, ppu->scanline);
    }
}

/* ============================================================================
 * PPU Initialization and Reset
 * ========================================================================== */

void ppu_init(NESPPU* ppu) {
    memset(ppu, 0, sizeof(NESPPU));

    /* Default to vertical mirroring */
    ppu->nametable_mirroring = 0;

    /* Initialize framebuffer to black */
    for (int i = 0; i < NES_FRAMEBUFFER_SIZE; i++) {
        ppu->framebuffer[i] = 0xFF000000;
    }

    ppu_reset(ppu);
    DBG_PPU("NES PPU initialized");
}

/**
 * @brief Load CHR data into PPU VRAM pattern tables
 * @param ppu PPU structure
 * @param chr_data Pointer to CHR data (from mapper)
 * @param chr_size Size of CHR data in bytes
 */
void ppu_load_chr(NESPPU* ppu, const uint8_t* chr_data, size_t chr_size) {
    if (!ppu || !chr_data) return;
    
    /* Copy CHR data to pattern tables (0x0000-0x1FFF) */
    size_t copy_size = (chr_size < 0x2000) ? chr_size : 0x2000;
    memcpy(ppu->vram, chr_data, copy_size);
    DBG_PPU("Loaded %zu bytes of CHR data into PPU VRAM", copy_size);
}

void ppu_reset(NESPPU* ppu) {
    /* Reset PPU registers to power-on state */
    ppu->ctrl = 0x00;
    ppu->mask = 0x00;
    ppu->status = 0x00;
    ppu->oam_addr = 0x00;
    ppu->oam_data = 0x00;
    ppu->vaddr = 0x0000;
    ppu->vaddr_latch = 0;
    ppu->read_buffer = 0x00;
    
    /* Internal state */
    ppu->write_toggle = 0;
    ppu->fine_x = 0;
    ppu->temp_vaddr = 0;
    ppu->sprite_count = 0;
    ppu->sprite_overflow = 0;
    
    /* Clear OAM */
    memset(ppu->oam, 0, sizeof(ppu->oam));

    /* Initialize palette with default colors (visible gray scale) */
    /* Background color (shared) - dark gray */
    ppu->palette[0x00] = 0x0D;  /* Dark gray */
    /* Background palettes 1-3 - light gray to white */
    ppu->palette[0x01] = 0x20;  /* Light gray */
    ppu->palette[0x02] = 0x30;  /* Lighter gray */
    ppu->palette[0x03] = 0x3D;  /* White */
    /* Copy to other background palettes */
    ppu->palette[0x04] = 0x0D;
    ppu->palette[0x05] = 0x20;
    ppu->palette[0x06] = 0x30;
    ppu->palette[0x07] = 0x3D;
    ppu->palette[0x08] = 0x0D;
    ppu->palette[0x09] = 0x20;
    ppu->palette[0x0A] = 0x30;
    ppu->palette[0x0B] = 0x3D;
    ppu->palette[0x0C] = 0x0D;
    ppu->palette[0x0D] = 0x20;
    ppu->palette[0x0E] = 0x30;
    ppu->palette[0x0F] = 0x3D;
    /* Sprite palettes - same as background */
    memcpy(&ppu->palette[0x10], &ppu->palette[0x00], 16);

    /* Clear VRAM */
    memset(ppu->vram, 0, sizeof(ppu->vram));

    /* TEMPORARY DEBUG: Fill nametable 0 with a visible pattern (tiles 0-63) */
    /* This is needed because game code doesn't initialize nametables */
    /* Fill nametable 0 (0x2000-0x23BF) with tiles 0-63 repeating */
    /* Nametables are stored at vram[0x2000+] in the VRAM array */
    for (int i = 0; i < 960; i++) {  /* 32 columns x 30 rows */
        ppu->vram[0x2000 + i] = i % 64;  /* Nametable 0 at vram[0x2000+] */
    }
    DBG_PPU("Initialized nametable 0 with tile pattern 0-63");

    /* Clear framebuffer with black */
    for (int i = 0; i < NES_FRAMEBUFFER_SIZE; i++) {
        ppu->framebuffer[i] = 0xFF000000;
    }
    
    /* Frame state - start at pre-render scanline */
    ppu->scanline = PPU_PRE_SCANLINE;
    ppu->cycle = 0;
    ppu->frame_number = 0;
    ppu->frame_ready = 0;
    ppu->flags = 0;
    ppu->nmi_requested = 0;
    
    DBG_PPU("NES PPU reset");
}

/* ============================================================================
 * PPU Tick Function
 * ========================================================================== */

void ppu_tick(NESPPU* ppu, NESContext* ctx, uint32_t cycles) {
    /* Recompiled code runs much faster than original hardware.
     * To prevent tight polling loops from starving the PPU,
     * we advance the PPU by a full frame per CPU cycle.
     * This is a hack, but it allows games to progress. */
    (void)cycles;
    /* Advance one full frame (262 scanlines * 341 cycles = 89342 PPU cycles) */
    for (uint32_t i = 0; i < 89342; i++) {
        ppu_step(ppu, ctx);
    }
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
                
                /* Reading status clears VBlank flag and write toggle */
                ppu->status &= ~PPUSTATUS_VBLANK;
                ppu->write_toggle = 0;
                
                /* Clear NMI pending */
                ppu->nmi_requested = 0;
                ppu->flags &= ~PPU_FLAG_NMI_PENDING;
                
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
                /* VRAM read - returns buffered data first, then loads new data */
                uint8_t data = ppu->read_buffer;
                
                /* Load buffer with data from current address */
                if (ppu->vaddr >= 0x3F00) {
                    /* Palette read - returns actual palette data immediately */
                    data = ppu_read_vram(ppu, ppu->vaddr);
                    ppu->read_buffer = ppu_read_vram(ppu, ppu->vaddr & 0x2FFF);
                } else {
                    /* VRAM read - returns previous buffer, loads new data */
                    ppu->read_buffer = ppu_read_vram(ppu, ppu->vaddr);
                }
                
                /* Increment vaddr */
                if (ppu->ctrl & PPUCTRL_INC_ADDR) {
                    ppu->vaddr += 32;
                } else {
                    ppu->vaddr += 1;
                }
                ppu->vaddr &= 0x3FFF;
                
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
            
            /* Update temp vaddr with nametable select bits */
            ppu->temp_vaddr &= ~0x0C00;
            ppu->temp_vaddr |= (value & PPUCTRL_BASE_NAMETABLE) << 10;
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
            if (ppu->write_toggle == 0) {
                /* First write: X scroll */
                ppu->fine_x = value & 0x07;
                ppu->temp_vaddr &= ~PPU_VADDR_COARSE_X;
                ppu->temp_vaddr |= (value >> 3) & PPU_VADDR_COARSE_X;
                ppu->write_toggle = 1;
            } else {
                /* Second write: Y scroll */
                ppu->temp_vaddr &= ~PPU_VADDR_FINE_Y;
                ppu->temp_vaddr |= (value & 0x07) << 12;
                ppu->temp_vaddr &= ~PPU_VADDR_COARSE_Y;
                ppu->temp_vaddr |= (value & 0xF8) << 2;
                ppu->write_toggle = 0;
            }
            break;
            
        case 0x06: /* $2006 - PPUADDR */
            if (ppu->write_toggle == 0) {
                /* First write: High byte */
                ppu->temp_vaddr &= 0x00FF;
                ppu->temp_vaddr |= (value & 0x3F) << 8;
                ppu->write_toggle = 1;
            } else {
                /* Second write: Low byte */
                ppu->temp_vaddr &= 0xFF00;
                ppu->temp_vaddr |= value;
                ppu->vaddr = ppu->temp_vaddr;
                ppu->write_toggle = 0;
            }
            break;
            
        case 0x07: /* $2007 - PPUDATA */
            ppu_write_vram(ppu, ppu->vaddr, value);
            
            /* Increment vaddr */
            if (ppu->ctrl & PPUCTRL_INC_ADDR) {
                ppu->vaddr += 32;
            } else {
                ppu->vaddr += 1;
            }
            ppu->vaddr &= 0x3FFF;
            break;
    }
}

/* ============================================================================
 * DMA Transfer
 * ========================================================================== */

void ppu_dma(NESPPU* ppu, const uint8_t* data) {
    /* Copy 256 bytes to OAM starting at oam_addr */
    for (int i = 0; i < 256; i++) {
        ppu->oam[(ppu->oam_addr + i) & 0xFF] = data[i];
    }
    DBG_PPU("OAM DMA transfer completed");
}

/* ============================================================================
 * Frame Handling
 * ========================================================================== */

bool ppu_frame_ready(NESPPU* ppu) {
    return ppu->frame_ready != 0;
}

void ppu_clear_frame_ready(NESPPU* ppu) {
    ppu->frame_ready = 0;
}

const uint32_t* ppu_get_framebuffer(NESPPU* ppu) {
    return ppu->framebuffer;
}

void ppu_set_mirroring(NESPPU* ppu, uint8_t mirroring) {
    ppu->nametable_mirroring = mirroring;
    DBG_PPU("PPU mirroring set to %s", mirroring ? "horizontal" : "vertical");
}
