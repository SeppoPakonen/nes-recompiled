/**
 * @file ppu.h
 * @brief NES PPU (Picture Processing Unit) stub header
 * 
 * NOTE: This is a stub implementation for compilation purposes.
 * Full NES PPU implementation will be added in task006.
 */

#ifndef NES_PPU_H
#define NES_PPU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

#define NES_SCREEN_WIDTH    256
#define NES_SCREEN_HEIGHT   240
#define NES_FRAMEBUFFER_SIZE (NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT)

/* PPU Control Register (PPUCTRL - $2000) */
#define PPUCTRL_BASE_NAMETABLE    0x03  /* Bits 0-1: Base nametable address */
#define PPUCTRL_INC_ADDR          0x04  /* Bit 2: VRAM address increment */
#define PPUCTRL_SPRITE_TABLE      0x08  /* Bit 3: Sprite pattern table address */
#define PPUCTRL_BG_TABLE          0x10  /* Bit 4: Background pattern table address */
#define PPUCTRL_SPRITE_SIZE       0x20  /* Bit 5: Sprite size (0=8x8, 1=8x16) */
#define PPUCTRL_MASTER_SLAVE      0x40  /* Bit 6: PPU master/slave select */
#define PPUCTRL_VBLANK_INT        0x80  /* Bit 7: VBlank interrupt enable */

/* PPU Mask Register (PPUMASK - $2001) */
#define PPUMASK_GRAYSCALE         0x01  /* Bit 0: Grayscale */
#define PPUMASK_SHOW_BG_LEFT      0x02  /* Bit 1: Show background in leftmost 8 pixels */
#define PPUMASK_SHOW_SPR_LEFT     0x04  /* Bit 2: Show sprites in leftmost 8 pixels */
#define PPUMASK_SHOW_BG           0x08  /* Bit 3: Show background */
#define PPUMASK_SHOW_SPR          0x10  /* Bit 4: Show sprites */
#define PPUMASK_EMPHASIS_RED      0x20  /* Bit 5: Emphasize red */
#define PPUMASK_EMPHASIS_GREEN    0x40  /* Bit 6: Emphasize green */
#define PPUMASK_EMPHASIS_BLUE     0x80  /* Bit 7: Emphasize blue */

/* PPU Status Register (PPUSTATUS - $2002) */
#define PPUSTATUS_SPRITE_OVERFLOW 0x20  /* Bit 5: Sprite overflow flag */
#define PPUSTATUS_SPRITE_ZERO_HIT 0x40  /* Bit 6: Sprite zero hit flag */
#define PPUSTATUS_VBLANK          0x80  /* Bit 7: VBlank flag */

/* OAM Entry (Sprite Attributes) - 4 bytes per sprite */
typedef struct {
    uint8_t y;          /* Y position */
    uint8_t tile;       /* Tile index */
    uint8_t attr;       /* Attributes (palette, flip, priority) */
    uint8_t x;          /* X position */
} OAMEntry;

/* OAM Attribute Flags */
#define OAM_ATTR_PALETTE      0x03  /* Bits 0-1: Palette */
#define OAM_ATTR_PRIORITY     0x20  /* Bit 5: Priority (0=front, 1=behind) */
#define OAM_ATTR_FLIP_X       0x40  /* Bit 6: X flip */
#define OAM_ATTR_FLIP_Y       0x80  /* Bit 7: Y flip */

/* ============================================================================
 * PPU State (Stub - to be implemented in task006)
 * ========================================================================== */

typedef struct NESContext NESContext;

typedef struct NESPPU {
    /* PPU Registers */
    uint8_t ctrl;         /* $2000 - PPU Control */
    uint8_t mask;         /* $2001 - PPU Mask */
    uint8_t status;       /* $2002 - PPU Status */
    uint8_t oam_addr;     /* $2003 - OAM Address */
    uint8_t oam_data;     /* $2004 - OAM Data */
    uint8_t scroll;       /* $2005 - Scroll */
    uint16_t vaddr;       /* $2006 - VRAM Address (15-bit) */
    uint8_t data;         /* $2007 - VRAM Data */
    
    /* Internal state */
    uint8_t vaddr_latch;  /* VRAM address latch for $2005/$2006 */
    uint8_t data_latch;   /* VRAM data read latch */
    uint8_t oam_sprite_count;  /* Sprites on current scanline */
    uint8_t oam_sprites[8];    /* Sprite indices for current scanline */
    
    /* OAM (Object Attribute Memory) - 256 bytes for 64 sprites */
    uint8_t oam[256];
    
    /* Palette memory - 32 bytes (8 background + 8 sprite palettes, 4 colors each) */
    uint8_t palette[32];
    
    /* VRAM - 2KB internal + nametables + pattern tables */
    uint8_t vram[2048];
    
    /* Framebuffer (RGB for display) */
    uint32_t rgb_framebuffer[NES_FRAMEBUFFER_SIZE];
    
    /* Frame complete flag */
    bool frame_ready;
    
    /* Scanline and cycle counters */
    int scanline;
    int cycle;

} NESPPU;

/* ============================================================================
 * PPU Functions (Stubs)
 * ========================================================================== */

/**
 * @brief Initialize PPU
 */
void ppu_init(NESPPU* ppu);

/**
 * @brief Reset PPU to initial state
 */
void ppu_reset(NESPPU* ppu);

/**
 * @brief Tick the PPU for a number of cycles
 */
void ppu_tick(NESPPU* ppu, NESContext* ctx, uint32_t cycles);

/**
 * @brief Read from PPU register
 */
uint8_t ppu_read_register(NESPPU* ppu, NESContext* ctx, uint16_t addr);

/**
 * @brief Write to PPU register
 */
void ppu_write_register(NESPPU* ppu, NESContext* ctx, uint16_t addr, uint8_t value);

/**
 * @brief Check if frame is ready
 */
bool ppu_frame_ready(NESPPU* ppu);

/**
 * @brief Clear frame ready flag
 */
void ppu_clear_frame_ready(NESPPU* ppu);

/**
 * @brief Get the RGB framebuffer
 */
const uint32_t* ppu_get_framebuffer(NESPPU* ppu);

#ifdef __cplusplus
}
#endif

#endif /* NES_PPU_H */
