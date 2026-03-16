/**
 * @file ppu.h
 * @brief NES PPU (Picture Processing Unit) implementation header
 *
 * Implements the NES RP2C02 Picture Processing Unit with:
 * - 2KB VRAM (nametables + pattern tables)
 * - 256 bytes OAM (64 sprites x 4 bytes)
 * - 32-byte palette RAM
 * - Scanline/cycle-based rendering
 */

#ifndef NES_PPU_H
#define NES_PPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ========================================================================== */

#define NES_SCREEN_WIDTH        256
#define NES_SCREEN_HEIGHT       240
#define NES_FRAMEBUFFER_SIZE    (NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT)

/* PPU timing constants */
#define PPU_CYCLES_PER_SCANLINE 341
#define PPU_SCANLINES_PER_FRAME 262
#define PPU_VISIBLE_SCANLINES   240
#define PPU_VBLANK_SCANLINE     241
#define PPU_POST_SCANLINE       240
#define PPU_PRE_SCANLINE        261

/* VRAM size */
#define PPU_VRAM_SIZE           0x3000  /* 12KB: 8KB pattern tables + 4KB nametables */
#define PPU_OAM_SIZE            256     /* 64 sprites x 4 bytes */
#define PPU_PALETTE_SIZE        32      /* 8 palettes x 4 colors */

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

/* OAM Attribute Flags */
#define OAM_ATTR_PALETTE      0x03  /* Bits 0-1: Palette */
#define OAM_ATTR_PRIORITY     0x20  /* Bit 5: Priority (0=front, 1=behind) */
#define OAM_ATTR_FLIP_X       0x40  /* Bit 6: X flip */
#define OAM_ATTR_FLIP_Y       0x80  /* Bit 7: Y flip */

/* Internal PPU flags */
#define PPU_FLAG_RENDER_ENABLED   0x01
#define PPU_FLAG_NMI_PENDING      0x02

/* VRAM address bit fields */
#define PPU_VADDR_COARSE_X      0x001F  /* Bits 0-4: Coarse X */
#define PPU_VADDR_COARSE_Y      0x03E0  /* Bits 5-9: Coarse Y */
#define PPU_VADDR_FINE_Y        0x7000  /* Bits 12-14: Fine Y */
#define PPU_VADDR_HORIZ_BIT     0x0400  /* Bit 10: Horizontal nametable select */
#define PPU_VADDR_VERT_BIT      0x0800  /* Bit 11: Vertical nametable select */

/* ============================================================================
 * PPU State
 * ========================================================================== */

typedef struct NESContext NESContext;

typedef struct NESPPU {
    /* PPU Registers (memory-mapped I/O) */
    uint8_t ctrl;         /* $2000 - PPU Control */
    uint8_t mask;         /* $2001 - PPU Mask */
    uint8_t status;       /* $2002 - PPU Status */
    uint8_t oam_addr;     /* $2003 - OAM Address */
    uint8_t oam_data;     /* $2004 - OAM Data (read/write) */
    uint8_t scroll_x;     /* $2005 - X Scroll (write-only, latched) */
    uint8_t scroll_y;     /* $2005 - Y Scroll (write-only, latched) */
    uint16_t vaddr;       /* $2006 - VRAM Address (15-bit) */
    uint8_t vaddr_latch;  /* $2006 - VRAM address write latch */
    uint8_t data;         /* $2007 - VRAM Data (read buffered) */

    /* VRAM - 12KB addressable space */
    /* 0x0000-0x1FFF: Pattern tables (CHR ROM/RAM) - 8KB */
    /* 0x2000-0x2FFF: Nametables and attribute tables - 4KB (mirrored) */
    /* Note: 0x3000-0x3EFF mirrors 0x2000-0x2EFF */
    /* Note: 0x3F00-0x3F1F is palette RAM (separate array) */
    uint8_t vram[PPU_VRAM_SIZE];

    /* OAM (Object Attribute Memory) - 256 bytes for 64 sprites */
    uint8_t oam[PPU_OAM_SIZE];

    /* Palette memory - 32 bytes */
    /* 0x00-0x0F: Background palettes (4 palettes x 4 colors) */
    /* 0x10-0x1F: Sprite palettes (4 palettes x 4 colors) */
    uint8_t palette[PPU_PALETTE_SIZE];

    /* Read buffer for VRAM reads */
    uint8_t read_buffer;

    /* Rendering state */
    int scanline;       /* Current scanline (0-261) */
    int cycle;          /* Current cycle within scanline (0-340) */
    int frame_number;   /* Frame counter */

    /* Scroll state */
    uint8_t fine_x;     /* Fine X scroll (0-7) */
    uint8_t write_toggle; /* $2005/$2006 write toggle */

    /* Temporary address register for scrolling */
    uint16_t temp_vaddr;

    /* Sprite evaluation for current scanline */
    uint8_t sprite_indices[8];  /* Up to 8 sprites per scanline */
    uint8_t sprite_count;       /* Number of sprites on current scanline */
    uint8_t sprite_overflow;    /* Sprite overflow flag */

    /* Framebuffer (ARGB8888) */
    uint32_t framebuffer[NES_FRAMEBUFFER_SIZE];

    /* Frame state */
    uint8_t flags;          /* Internal flags */
    uint8_t frame_ready;    /* Frame complete flag */
    uint8_t nmi_requested;  /* NMI requested flag */
    uint32_t last_status_read_frame; /* Last frame when PPUSTATUS was read */

    /* Nametable mirroring */
    uint8_t nametable_mirroring; /* 0=vertical, 1=horizontal */

} NESPPU;

/* ============================================================================
 * PPU Functions
 * ========================================================================== */

/**
 * @brief Initialize PPU
 * @param ppu PPU structure to initialize
 */
void ppu_init(NESPPU* ppu);

/**
 * @brief Reset PPU to initial state
 * @param ppu PPU structure to reset
 */
void ppu_reset(NESPPU* ppu);

/**
 * @brief Tick the PPU for a number of cycles
 * @param ppu PPU structure
 * @param ctx NES context (for NMI generation)
 * @param cycles Number of cycles to advance
 */
void ppu_tick(NESPPU* ppu, NESContext* ctx, uint32_t cycles);

/**
 * @brief Read from PPU register
 * @param ppu PPU structure
 * @param ctx NES context
 * @param addr Register address ($2000-$2007)
 * @return Value read from register
 */
uint8_t ppu_read_register(NESPPU* ppu, NESContext* ctx, uint16_t addr);

/**
 * @brief Write to PPU register
 * @param ppu PPU structure
 * @param ctx NES context
 * @param addr Register address ($2000-$2007)
 * @param value Value to write
 */
void ppu_write_register(NESPPU* ppu, NESContext* ctx, uint16_t addr, uint8_t value);

/**
 * @brief Perform OAM DMA transfer
 * @param ppu PPU structure
 * @param data Pointer to 256 bytes of OAM data
 */
void ppu_dma(NESPPU* ppu, const uint8_t* data);

/**
 * @brief Check if frame is ready
 * @param ppu PPU structure
 * @return true if frame is complete
 */
bool ppu_frame_ready(NESPPU* ppu);

/**
 * @brief Clear frame ready flag
 * @param ppu PPU structure
 */
void ppu_clear_frame_ready(NESPPU* ppu);

/**
 * @brief Get the framebuffer
 * @param ppu PPU structure
 * @return Pointer to ARGB8888 framebuffer
 */
const uint32_t* ppu_get_framebuffer(NESPPU* ppu);

/**
 * @brief Set nametable mirroring mode
 * @param ppu PPU structure
 * @param mirroring 0=vertical, 1=horizontal
 */
void ppu_set_mirroring(NESPPU* ppu, uint8_t mirroring);

/**
 * @brief Load CHR data into PPU VRAM pattern tables
 * @param ppu PPU structure
 * @param chr_data Pointer to CHR data (from mapper)
 * @param chr_size Size of CHR data in bytes
 */
void ppu_load_chr(NESPPU* ppu, const uint8_t* chr_data, size_t chr_size);

/**
 * @brief Render a scanline
 * @param ppu PPU structure
 * @param scanline Scanline number to render
 */
void ppu_render_scanline(NESPPU* ppu, int scanline);

#ifdef __cplusplus
}
#endif

#endif /* NES_PPU_H */
