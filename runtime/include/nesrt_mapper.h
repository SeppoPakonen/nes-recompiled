/**
 * @file nesrt_mapper.h
 * @brief NES Mapper interface for ROM banking
 *
 * Handles NES cartridge mappers for PRG/CHR banking and mirroring control.
 * Supports common mappers: NROM (0), MMC1 (1), UxROM (2), CNROM (3), MMC3 (4).
 */

#ifndef NESRT_MAPPER_H
#define NESRT_MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Mapper Types
 * ========================================================================== */

/**
 * @brief Supported mapper types
 */
typedef enum {
    MAPPER_NROM = 0,    /**< NROM - No banking */
    MAPPER_MMC1 = 1,    /**< MMC1 - Serial shift register */
    MAPPER_UXROM = 2,   /**< UxROM - Simple PRG banking */
    MAPPER_CNROM = 3,   /**< CNROM - Simple CHR banking */
    MAPPER_MMC3 = 4,    /**< MMC3 - Complex banking with IRQ */
} NESMapperType;

/**
 * @brief Mirroring modes
 */
typedef enum {
    MIRROR_HORIZONTAL = 0,  /**< Horizontal mirroring */
    MIRROR_VERTICAL = 1,    /**< Vertical mirroring */
    MIRROR_FOUR_SCREEN = 2, /**< Four-screen VRAM */
    MIRROR_SINGLE_0 = 3,    /**< Single-screen, use nametable 0 */
    MIRROR_SINGLE_1 = 4,    /**< Single-screen, use nametable 1 */
} NESMirroringMode;

/* ============================================================================
 * Mapper-Specific State Structures
 * ========================================================================== */

/**
 * @brief MMC1 internal state
 *
 * MMC1 uses a serial shift register for writes.
 * Bits are shifted in one at a time via writes to $8000-$FFFF.
 */
typedef struct {
    uint8_t shift_register; /**< 5-bit shift register */
    uint8_t write_count;    /**< Number of bits written (0-4) */
    uint8_t control;        /**< Control register ($8000) */
    uint8_t chr_bank_0;     /**< CHR bank for $0000-$0FFF ($A000) */
    uint8_t chr_bank_1;     /**< CHR bank for $1000-$1FFF ($C000) */
    uint8_t prg_bank;       /**< PRG bank ($B000) */
    uint8_t prg_ram_enabled;/**< PRG RAM enabled */
} NESMapperMMC1;

/**
 * @brief MMC3 internal state
 */
typedef struct {
    uint8_t bank_select;    /**< Bank select register ($8000) */
    uint8_t bank_registers[6]; /**< Bank value registers ($8001) */
    uint8_t prg_ram_protect; /**< PRG RAM protect ($A000) */
    uint8_t irq_latch;      /**< IRQ latch ($C000) */
    uint8_t irq_reload;     /**< IRQ reload ($C001) */
    uint8_t irq_counter;    /**< IRQ counter */
    uint8_t irq_enabled;    /**< IRQ enabled flag ($E001) */
    uint8_t irq_pending;    /**< IRQ pending flag */
} NESMapperMMC3;

/* ============================================================================
 * Main Mapper Structure
 * ========================================================================== */

/**
 * @brief NES Mapper state
 *
 * Contains all state needed for ROM banking and mirroring.
 */
typedef struct {
    /* Basic info */
    NESMapperType type;         /**< Mapper type from ROM header */
    uint8_t prg_banks;          /**< Number of 16KB PRG ROM banks */
    uint8_t chr_banks;          /**< Number of 8KB CHR ROM/RAM banks */

    /* Banking state */
    uint8_t prg_bank_0;         /**< PRG bank for $8000-$BFFF (or fixed) */
    uint8_t prg_bank_1;         /**< PRG bank for $C000-$FFFF */
    uint8_t chr_bank_0;         /**< CHR bank for $0000-$0FFF */
    uint8_t chr_bank_1;         /**< CHR bank for $1000-$1FFF */

    /* Mirroring */
    NESMirroringMode mirroring; /**< Current mirroring mode */

    /* PRG RAM */
    uint8_t prg_ram_enabled;    /**< PRG RAM enabled for writes */
    uint8_t prg_ram_bank;       /**< Current PRG RAM bank (if supported) */
    uint8_t* prg_ram;           /**< Pointer to PRG RAM (battery-backed) */
    size_t prg_ram_size;        /**< Size of PRG RAM in bytes */

    /* CHR RAM (if no CHR ROM) */
    uint8_t* chr_ram;           /**< Pointer to CHR RAM */
    size_t chr_ram_size;        /**< Size of CHR RAM */
    bool chr_is_ram;            /**< True if CHR is RAM, false if ROM */

    /* Mapper-specific state */
    union {
        NESMapperMMC1 mmc1;     /**< MMC1 state */
        NESMapperMMC3 mmc3;     /**< MMC3 state */
        uint8_t reserved[64];   /**< Reserved space for other mappers */
    };
} NESMapper;

/* ============================================================================
 * ROM Data Structure for Mapper
 * ========================================================================== */

/**
 * @brief ROM data pointers for mapper
 */
typedef struct {
    const uint8_t* prg_rom;     /**< PRG ROM data */
    size_t prg_rom_size;        /**< PRG ROM size in bytes */
    const uint8_t* chr_rom;     /**< CHR ROM data (NULL if CHR RAM) */
    size_t chr_rom_size;        /**< CHR ROM size in bytes */
} NESROMData;

/* ============================================================================
 * Mapper API Functions
 * ========================================================================== */

/**
 * @brief Initialize mapper based on ROM header
 * @param mapper Mapper state to initialize
 * @param mapper_number Mapper number from iNES header
 * @param prg_banks Number of 16KB PRG ROM banks
 * @param chr_banks Number of 8KB CHR ROM/RAM banks
 * @param prg_ram Pointer to PRG RAM (may be NULL)
 * @param prg_ram_size Size of PRG RAM (0 if none)
 * @param chr_ram Pointer to CHR RAM (may be NULL if CHR ROM)
 * @param chr_ram_size Size of CHR RAM
 * @return true on success
 */
bool nes_mapper_init(NESMapper* mapper,
                     uint8_t mapper_number,
                     uint8_t prg_banks,
                     uint8_t chr_banks,
                     uint8_t* prg_ram,
                     size_t prg_ram_size,
                     uint8_t* chr_ram,
                     size_t chr_ram_size);

/**
 * @brief Read from PRG ROM with bank switching
 * @param mapper Mapper state
 * @param rom_data ROM data pointers
 * @param addr Address in CPU address space ($8000-$FFFF)
 * @return Byte at address
 */
uint8_t nes_mapper_prg_read(NESMapper* mapper,
                            const NESROMData* rom_data,
                            uint16_t addr);

/**
 * @brief Write to mapper registers ($6000-$FFFF)
 * @param mapper Mapper state
 * @param addr Write address
 * @param value Value to write
 */
void nes_mapper_write(NESMapper* mapper, uint16_t addr, uint8_t value);

/**
 * @brief Get pointer to PRG bank at address
 * @param mapper Mapper state
 * @param rom_data ROM data pointers
 * @param addr Address in CPU address space ($8000-$FFFF)
 * @return Pointer to byte in PRG ROM/RAM, or NULL if invalid
 */
const uint8_t* nes_mapper_get_prg_ptr(NESMapper* mapper,
                                      const NESROMData* rom_data,
                                      uint16_t addr);

/**
 * @brief Get pointer to CHR bank at address
 * @param mapper Mapper state
 * @param rom_data ROM data pointers
 * @param addr Address in PPU address space ($0000-$1FFF)
 * @return Pointer to byte in CHR ROM/RAM, or NULL if invalid
 */
const uint8_t* nes_mapper_get_chr_ptr(NESMapper* mapper,
                                      const NESROMData* rom_data,
                                      uint16_t addr);

/**
 * @brief Check if address is PRG RAM
 * @param mapper Mapper state
 * @param addr Address to check ($6000-$7FFF or $8000-$FFFF for some mappers)
 * @return true if address maps to PRG RAM
 */
bool nes_mapper_is_prg_ram(NESMapper* mapper, uint16_t addr);

/**
 * @brief Read from PRG RAM
 * @param mapper Mapper state
 * @param addr Address in PRG RAM space
 * @return Byte at address, or 0xFF if invalid
 */
uint8_t nes_mapper_prg_ram_read(NESMapper* mapper, uint16_t addr);

/**
 * @brief Write to PRG RAM
 * @param mapper Mapper state
 * @param addr Address in PRG RAM space
 * @param value Value to write
 */
void nes_mapper_prg_ram_write(NESMapper* mapper, uint16_t addr, uint8_t value);

/**
 * @brief Get mirroring mode
 * @param mapper Mapper state
 * @return Current mirroring mode
 */
NESMirroringMode nes_mapper_get_mirroring(NESMapper* mapper);

/**
 * @brief Check if mapper generates IRQs
 * @param mapper Mapper state
 * @return true if mapper can generate IRQs
 */
bool nes_mapper_has_irq(NESMapper* mapper);

/**
 * @brief Tick mapper (for IRQ counters)
 * @param mapper Mapper state
 * @param scanline Current PPU scanline (0-261)
 */
void nes_mapper_tick(NESMapper* mapper, int scanline);

/**
 * @brief Get IRQ pending flag
 * @param mapper Mapper state
 * @return true if IRQ is pending
 */
bool nes_mapper_irq_pending(NESMapper* mapper);

/**
 * @brief Clear IRQ pending flag
 * @param mapper Mapper state
 */
void nes_mapper_irq_clear(NESMapper* mapper);

/**
 * @brief Set PRG bank for code execution (for generated code bank switching)
 * @param mapper Mapper state
 * @param bank PRG bank number (0-7 for MMC1)
 * @param addr Address where bank switch occurs (for logging)
 */
void nes_mapper_set_prg_bank(NESMapper* mapper, uint8_t bank, uint16_t addr);

/**
 * @brief Get current PRG bank
 * @param mapper Mapper state
 * @return Current PRG bank number
 */
uint8_t nes_mapper_get_prg_bank(NESMapper* mapper);

#ifdef __cplusplus
}
#endif

#endif /* NESRT_MAPPER_H */
