/**
 * @file mapper.c
 * @brief NES Mapper implementation
 *
 * Implements PRG/CHR banking and mirroring for common NES mappers.
 */

#include "nesrt_mapper.h"
#include "nesrt_trace.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief Get PRG bank mask based on number of banks
 */
static inline uint8_t get_prg_bank_mask(uint8_t prg_banks) {
    if (prg_banks <= 1) return 0x00;
    if (prg_banks <= 2) return 0x01;
    if (prg_banks <= 4) return 0x03;
    if (prg_banks <= 8) return 0x07;
    if (prg_banks <= 16) return 0x0F;
    if (prg_banks <= 32) return 0x1F;
    if (prg_banks <= 64) return 0x3F;
    if (prg_banks <= 128) return 0x7F;
    return 0xFF;
}

/**
 * @brief Get CHR bank mask based on number of banks
 */
static inline uint8_t get_chr_bank_mask(uint8_t chr_banks) {
    if (chr_banks <= 1) return 0x00;
    if (chr_banks <= 2) return 0x01;
    if (chr_banks <= 4) return 0x03;
    if (chr_banks <= 8) return 0x07;
    if (chr_banks <= 16) return 0x0F;
    if (chr_banks <= 32) return 0x1F;
    if (chr_banks <= 64) return 0x3F;
    return 0x7F;
}

/* ============================================================================
 * Mapper Initialization
 * ========================================================================== */

bool nes_mapper_init(NESMapper* mapper,
                     uint8_t mapper_number,
                     uint8_t prg_banks,
                     uint8_t chr_banks,
                     uint8_t* prg_ram,
                     size_t prg_ram_size,
                     uint8_t* chr_ram,
                     size_t chr_ram_size) {
    if (!mapper) return false;

    memset(mapper, 0, sizeof(NESMapper));

    /* Set mapper type */
    switch (mapper_number) {
        case 0:  mapper->type = MAPPER_NROM; break;
        case 1:  mapper->type = MAPPER_MMC1; break;
        case 2:  mapper->type = MAPPER_UXROM; break;
        case 3:  mapper->type = MAPPER_CNROM; break;
        case 4:  mapper->type = MAPPER_MMC3; break;
        default:
            /* Unsupported mapper - fall back to NROM */
            fprintf(stderr, "[MAPPER] Unsupported mapper %d, using NROM\n", mapper_number);
            mapper->type = MAPPER_NROM;
            break;
    }

    mapper->prg_banks = prg_banks;
    mapper->chr_banks = chr_banks;

    /* Set up PRG RAM */
    mapper->prg_ram = prg_ram;
    mapper->prg_ram_size = prg_ram_size;
    mapper->prg_ram_enabled = (prg_ram != NULL && prg_ram_size > 0);

    /* Set up CHR RAM */
    mapper->chr_ram = chr_ram;
    mapper->chr_ram_size = chr_ram_size;
    mapper->chr_is_ram = (chr_ram != NULL && chr_ram_size > 0);

    /* Initialize banking based on mapper type */
    switch (mapper->type) {
        case MAPPER_NROM:
            /* NROM: Fixed banks, no switching */
            mapper->prg_bank_0 = 0;
            mapper->prg_bank_1 = (prg_banks > 1) ? 1 : 0;
            mapper->chr_bank_0 = 0;
            mapper->chr_bank_1 = 0;
            mapper->mirroring = MIRROR_VERTICAL; /* Default */
            break;

        case MAPPER_MMC1:
            /* MMC1: Initialize control register */
            mapper->mmc1.control = 0x0C; /* PRG RAM enabled, 32KB PRG mode */
            mapper->mmc1.prg_ram_enabled = 1;
            mapper->mmc1.shift_register = 0;
            mapper->mmc1.write_count = 0;
            mapper->prg_bank_0 = 0;
            mapper->prg_bank_1 = 0;
            mapper->chr_bank_0 = 0;
            mapper->chr_bank_1 = 0;
            mapper->mirroring = MIRROR_VERTICAL;
            break;

        case MAPPER_UXROM:
            /* UxROM: Bank at $8000-$BFFF, fixed bank at $C000-$FFFF */
            mapper->prg_bank_0 = 0;
            mapper->prg_bank_1 = prg_banks - 1; /* Last bank fixed */
            mapper->chr_bank_0 = 0;
            mapper->chr_bank_1 = 0;
            mapper->mirroring = MIRROR_VERTICAL;
            break;

        case MAPPER_CNROM:
            /* CNROM: Fixed PRG, switchable CHR */
            mapper->prg_bank_0 = 0;
            mapper->prg_bank_1 = (prg_banks > 1) ? 1 : 0;
            mapper->chr_bank_0 = 0;
            mapper->chr_bank_1 = 0;
            mapper->mirroring = MIRROR_VERTICAL;
            break;

        case MAPPER_MMC3:
            /* MMC3: Complex initialization */
            mapper->mmc3.bank_select = 0;
            mapper->mmc3.prg_ram_protect = 0;
            mapper->mmc3.irq_enabled = 0;
            mapper->mmc3.irq_pending = 0;
            mapper->prg_bank_0 = 0;
            mapper->prg_bank_1 = 0;
            mapper->chr_bank_0 = 0;
            mapper->chr_bank_1 = 0;
            mapper->mirroring = MIRROR_VERTICAL;
            break;

        default:
            break;
    }

    return true;
}

/* ============================================================================
 * PRG ROM Read with Bank Switching
 * ========================================================================== */

uint8_t nes_mapper_prg_read(NESMapper* mapper,
                            const NESROMData* rom_data,
                            uint16_t addr) {
    if (!mapper || !rom_data || addr < 0x8000) {
        return 0xFF;
    }

    /* Check for PRG RAM access ($6000-$7FFF) */
    if (addr < 0x8000) {
        if (mapper->prg_ram && mapper->prg_ram_enabled) {
            if ((size_t)(addr - 0x6000) < mapper->prg_ram_size) {
                return mapper->prg_ram[addr - 0x6000];
            }
        }
        return 0xFF; /* Open bus */
    }

    /* PRG ROM access ($8000-$FFFF) */
    uint32_t rom_addr;
    uint8_t bank;

    switch (mapper->type) {
        case MAPPER_NROM:
            /* NROM: No banking, direct mapping */
            if (mapper->prg_banks == 1) {
                /* 16KB ROM: mirrored in both regions */
                rom_addr = addr & 0x3FFF;
            } else {
                /* 32KB ROM: $8000-$BFFF = bank 0, $C000-$FFFF = bank 1 */
                rom_addr = addr - 0x8000;
            }
            break;

        case MAPPER_MMC1:
            /* MMC1: Banking depends on control register */
            if (mapper->mmc1.control & 0x08) {
                /* 32KB PRG mode: prg_bank is base, ignores low bit */
                bank = mapper->mmc1.prg_bank & 0xFE;
                rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0x8000);
            } else {
                /* 16KB PRG mode */
                if (addr < 0xC000) {
                    /* $8000-$BFFF: Switchable */
                    bank = mapper->mmc1.prg_bank;
                    rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0x8000);
                } else {
                    /* $C000-$FFFF: Fixed to last bank */
                    bank = mapper->prg_banks - 1;
                    rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0xC000);
                }
            }
            break;

        case MAPPER_UXROM:
            /* UxROM: $8000-$BFFF switchable, $C000-$FFFF fixed to last bank */
            if (addr < 0xC000) {
                bank = mapper->prg_bank_0;
                rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0x8000);
            } else {
                bank = mapper->prg_banks - 1;
                rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0xC000);
            }
            break;

        case MAPPER_CNROM:
            /* CNROM: No PRG banking */
            rom_addr = addr - 0x8000;
            break;

        case MAPPER_MMC3:
            /* MMC3: Complex banking with 8KB banks */
            /* Simplified: using 16KB banks for now */
            if (addr < 0xC000) {
                bank = mapper->prg_bank_0;
                rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0x8000);
            } else {
                bank = mapper->prg_bank_1;
                rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0xC000);
            }
            break;

        default:
            rom_addr = addr - 0x8000;
            break;
    }

    /* Bounds check */
    if (rom_addr >= rom_data->prg_rom_size) {
        return 0xFF;
    }

    return rom_data->prg_rom[rom_addr];
}

/* ============================================================================
 * Mapper Register Writes ($8000-$FFFF)
 * ========================================================================== */

void nes_mapper_write(NESMapper* mapper, uint16_t addr, uint8_t value) {
    if (!mapper) return;

    /* Only handle writes to mapper registers ($8000-$FFFF) */
    if (addr < 0x8000) {
        /* $6000-$7FFF: PRG RAM (handled separately) */
        return;
    }

    /* Add tracing for bank switch events */
    if (nesrt_trace_is_enabled()) {
        nesrt_set_tag("mapper");
        nesrt_set_tag("bank-switch");
    }

    switch (mapper->type) {
        case MAPPER_NROM:
            /* NROM: No registers, writes ignored */
            break;

        case MAPPER_MMC1: {
            /* MMC1: Serial shift register */
            NESMapperMMC1* mmc1 = &mapper->mmc1;

            /* Debug logging for MMC1 writes */
            fprintf(stderr, "[MMC1] Write $%04X = $%02X (count=%d, shift=%d)\n",
                    addr, value, mmc1->write_count, mmc1->shift_register);

            if (value & 0x80) {
                /* Reset */
                mmc1->shift_register = 0;
                mmc1->write_count = 0;
                mmc1->control |= 0x0C;
                fprintf(stderr, "[MMC1] RESET\n");
                if (nesrt_trace_is_enabled()) {
                    nesrt_clear_tag("bank-switch");
                    nesrt_clear_tag("mapper");
                }
                return;
            }

            /* Shift in bit */
            mmc1->shift_register |= (value & 0x01) << mmc1->write_count;
            mmc1->write_count++;

            if (mmc1->write_count < 5) {
                if (nesrt_trace_is_enabled()) {
                    nesrt_clear_tag("bank-switch");
                    nesrt_clear_tag("mapper");
                }
                return; /* Wait for 5 bits */
            }

            /* Latch the 5-bit value */
            uint8_t reg_value = mmc1->shift_register;
            fprintf(stderr, "[MMC1] LATCHED: $%02X (addr $%04X)\n", reg_value, addr);

            mmc1->shift_register = 0;
            mmc1->write_count = 0;

            /* Determine register by address */
            uint16_t reg_addr = addr & 0xE000; /* Decode address range */

            switch (reg_addr) {
                case 0x8000:
                    /* Control register */
                    fprintf(stderr, "[MMC1] Control reg: $%02X\n", reg_value);
                    mmc1->control = reg_value;
                    /* Update mirroring from control bits */
                    switch ((reg_value >> 2) & 0x03) {
                        case 0: mapper->mirroring = MIRROR_SINGLE_0; break;
                        case 1: mapper->mirroring = MIRROR_SINGLE_1; break;
                        case 2: mapper->mirroring = MIRROR_VERTICAL; break;
                        case 3: mapper->mirroring = MIRROR_HORIZONTAL; break;
                    }
                    break;

                case 0xA000:
                    /* CHR bank 0 ($0000-$0FFF) */
                    fprintf(stderr, "[MMC1] CHR bank 0: $%02X\n", reg_value);
                    if (mapper->chr_is_ram) {
                        mapper->chr_bank_0 = reg_value & 0x1F; /* 5 bits for 8KB banks */
                    } else {
                        mapper->chr_bank_0 = reg_value;
                    }
                    if (nesrt_trace_is_enabled()) {
                        nesrt_set_tag("chr");
                        nesrt_log_bank_switch(mapper->type == MAPPER_MMC1 ? 1 : mapper->type, 
                                              mapper->chr_bank_0, addr, 3);
                        nesrt_clear_tag("chr");
                    }
                    break;

                case 0xC000:
                    /* CHR bank 1 ($1000-$1FFF) */
                    fprintf(stderr, "[MMC1] CHR bank 1: $%02X\n", reg_value);
                    if (mapper->chr_is_ram) {
                        mapper->chr_bank_1 = reg_value & 0x1F;
                    } else {
                        mapper->chr_bank_1 = reg_value;
                    }
                    if (nesrt_trace_is_enabled()) {
                        nesrt_set_tag("chr");
                        nesrt_log_bank_switch(mapper->type == MAPPER_MMC1 ? 1 : mapper->type,
                                              mapper->chr_bank_1, addr, 3);
                        nesrt_clear_tag("chr");
                    }
                    break;

                case 0xE000:
                    /* PRG bank */
                    fprintf(stderr, "[MMC1] PRG bank: $%02X\n", reg_value);
                    mmc1->prg_bank = reg_value & 0x0F;
                    if (nesrt_trace_is_enabled()) {
                        nesrt_set_tag("prg");
                        nesrt_log_bank_switch(mapper->type == MAPPER_MMC1 ? 1 : mapper->type,
                                              mmc1->prg_bank, addr, 3);
                        nesrt_clear_tag("prg");
                    }
                    break;
            }
            break;
        }

        case MAPPER_UXROM:
            /* UxROM: Simple PRG bank register at $8000-$FFFF */
            mapper->prg_bank_0 = value;
            if (nesrt_trace_is_enabled()) {
                nesrt_set_tag("prg");
                nesrt_log_bank_switch(MAPPER_UXROM, mapper->prg_bank_0, addr, 3);
                nesrt_clear_tag("prg");
            }
            break;

        case MAPPER_CNROM:
            /* CNROM: CHR bank register (lower 3-4 bits) */
            mapper->chr_bank_0 = value & 0x0F;
            mapper->chr_bank_1 = 0; /* CNROM only has one CHR bank register */
            if (nesrt_trace_is_enabled()) {
                nesrt_set_tag("chr");
                nesrt_log_bank_switch(MAPPER_CNROM, mapper->chr_bank_0, addr, 3);
                nesrt_clear_tag("chr");
            }
            break;

        case MAPPER_MMC3: {
            /* MMC3: Complex register set */
            NESMapperMMC3* mmc3 = &mapper->mmc3;

            uint16_t reg_addr = addr & 0xE001; /* Decode with A0 */

            switch (reg_addr) {
                case 0x8000:
                    /* Bank select */
                    mmc3->bank_select = value;
                    break;

                case 0x8001:
                    /* Bank data - depends on bank_select */
                    {
                        uint8_t bank_num = mmc3->bank_select & 0x07;
                        if (bank_num < 6) {
                            mmc3->bank_registers[bank_num] = value;
                            if (nesrt_trace_is_enabled()) {
                                if (bank_num < 4) {
                                    nesrt_set_tag("chr");
                                } else {
                                    nesrt_set_tag("prg");
                                }
                                nesrt_log_bank_switch(MAPPER_MMC3, value, addr, 3);
                                nesrt_clear_tag("chr");
                                nesrt_clear_tag("prg");
                            }
                        }
                    }
                    break;

                case 0xA000:
                    /* PRG RAM protect */
                    mmc3->prg_ram_protect = value;
                    break;

                case 0xC000:
                    /* IRQ latch */
                    mmc3->irq_latch = value;
                    break;

                case 0xC001:
                    /* IRQ reload */
                    mmc3->irq_reload = value;
                    if (value) {
                        mmc3->irq_counter = 0;
                    }
                    break;

                case 0xE000:
                    /* IRQ disable */
                    mmc3->irq_enabled = 0;
                    break;

                case 0xE001:
                    /* IRQ enable */
                    mmc3->irq_enabled = 1;
                    break;
            }
            break;
        }

        default:
            break;
    }

    if (nesrt_trace_is_enabled()) {
        nesrt_clear_tag("bank-switch");
        nesrt_clear_tag("mapper");
    }
}

/* ============================================================================
 * Pointer Access Functions
 * ========================================================================== */

const uint8_t* nes_mapper_get_prg_ptr(NESMapper* mapper,
                                      const NESROMData* rom_data,
                                      uint16_t addr) {
    if (!mapper || !rom_data || addr < 0x8000) {
        return NULL;
    }

    uint32_t rom_addr;

    switch (mapper->type) {
        case MAPPER_NROM:
            if (mapper->prg_banks == 1) {
                rom_addr = addr & 0x3FFF;
            } else {
                rom_addr = addr - 0x8000;
            }
            break;

        case MAPPER_MMC1:
            if (mapper->mmc1.control & 0x08) {
                uint8_t bank = mapper->mmc1.prg_bank & 0xFE;
                rom_addr = ((uint32_t)bank * 0x4000) + (addr - 0x8000);
            } else {
                if (addr < 0xC000) {
                    rom_addr = ((uint32_t)mapper->mmc1.prg_bank * 0x4000) + (addr - 0x8000);
                } else {
                    rom_addr = ((uint32_t)(mapper->prg_banks - 1) * 0x4000) + (addr - 0xC000);
                }
            }
            break;

        case MAPPER_UXROM:
            if (addr < 0xC000) {
                rom_addr = ((uint32_t)mapper->prg_bank_0 * 0x4000) + (addr - 0x8000);
            } else {
                rom_addr = ((uint32_t)(mapper->prg_banks - 1) * 0x4000) + (addr - 0xC000);
            }
            break;

        case MAPPER_CNROM:
            rom_addr = addr - 0x8000;
            break;

        case MAPPER_MMC3:
            if (addr < 0xC000) {
                rom_addr = ((uint32_t)mapper->prg_bank_0 * 0x4000) + (addr - 0x8000);
            } else {
                rom_addr = ((uint32_t)mapper->prg_bank_1 * 0x4000) + (addr - 0xC000);
            }
            break;

        default:
            rom_addr = addr - 0x8000;
            break;
    }

    if (rom_addr >= rom_data->prg_rom_size) {
        return NULL;
    }

    return &rom_data->prg_rom[rom_addr];
}

const uint8_t* nes_mapper_get_chr_ptr(NESMapper* mapper,
                                      const NESROMData* rom_data,
                                      uint16_t addr) {
    if (!mapper || addr >= 0x2000) {
        return NULL;
    }

    uint32_t chr_addr;
    uint8_t bank;

    /* Determine which CHR bank to use based on mapper type and mode */
    if (mapper->type == MAPPER_MMC1) {
        /* MMC1 has two CHR bank modes controlled by bit 0 of control register:
         * - 8KB mode (bit 0 = 0): chr_bank_0 controls entire $0000-$1FFF range
         * - 4KB mode (bit 0 = 1): chr_bank_0 controls $0000-$0FFF, chr_bank_1 controls $1000-$1FFF
         */
        bool chr_4kb_mode = mapper->mmc1.control & 0x01;
        
        if (chr_4kb_mode) {
            /* 4KB mode: separate banks for each 4KB region */
            if (addr < 0x1000) {
                bank = mapper->chr_bank_0;
            } else {
                bank = mapper->chr_bank_1;
                addr -= 0x1000;
            }
        } else {
            /* 8KB mode: chr_bank_0 controls entire $0000-$1FFF range */
            bank = mapper->chr_bank_0;
            /* Don't subtract - addr stays as-is for 8KB bank calculation */
        }
    } else {
        /* Other mappers: use simple 4KB bank switching */
        if (addr < 0x1000) {
            bank = mapper->chr_bank_0;
        } else {
            bank = mapper->chr_bank_1;
            addr -= 0x1000;
        }
    }

    /* Calculate address within CHR space */
    if (mapper->chr_is_ram) {
        /* CHR RAM: always 4KB banks */
        chr_addr = ((uint32_t)bank * 0x1000) + addr;
        if (chr_addr >= mapper->chr_ram_size) {
            return NULL;
        }
        return &mapper->chr_ram[chr_addr];
    } else {
        /* CHR ROM: bank size depends on mode */
        uint32_t bank_size = (mapper->type == MAPPER_MMC1 && !(mapper->mmc1.control & 0x01)) 
                             ? 0x2000 : 0x1000;
        chr_addr = ((uint32_t)bank * bank_size) + addr;
        if (!rom_data->chr_rom || chr_addr >= rom_data->chr_rom_size) {
            return NULL;
        }
        return &rom_data->chr_rom[chr_addr];
    }
}

/* ============================================================================
 * PRG RAM Access
 * ========================================================================== */

bool nes_mapper_is_prg_ram(NESMapper* mapper, uint16_t addr) {
    if (!mapper) return false;

    /* $6000-$7FFF is typically PRG RAM */
    if (addr >= 0x6000 && addr < 0x8000) {
        return mapper->prg_ram != NULL && mapper->prg_ram_enabled;
    }

    return false;
}

uint8_t nes_mapper_prg_ram_read(NESMapper* mapper, uint16_t addr) {
    if (!mapper || !mapper->prg_ram || !mapper->prg_ram_enabled) {
        return 0xFF;
    }

    if (addr >= 0x6000 && addr < 0x8000) {
        uint32_t ram_addr = addr - 0x6000;
        if (ram_addr < mapper->prg_ram_size) {
            return mapper->prg_ram[ram_addr];
        }
    }

    return 0xFF;
}

void nes_mapper_prg_ram_write(NESMapper* mapper, uint16_t addr, uint8_t value) {
    if (!mapper || !mapper->prg_ram || !mapper->prg_ram_enabled) {
        return;
    }

    if (addr >= 0x6000 && addr < 0x8000) {
        uint32_t ram_addr = addr - 0x6000;
        if (ram_addr < mapper->prg_ram_size) {
            mapper->prg_ram[ram_addr] = value;
        }
    }
}

/* ============================================================================
 * Mirroring and IRQ
 * ========================================================================== */

NESMirroringMode nes_mapper_get_mirroring(NESMapper* mapper) {
    if (!mapper) return MIRROR_VERTICAL;
    return mapper->mirroring;
}

bool nes_mapper_has_irq(NESMapper* mapper) {
    if (!mapper) return false;
    /* Only MMC3 has IRQ among supported mappers */
    return mapper->type == MAPPER_MMC3;
}

void nes_mapper_tick(NESMapper* mapper, int scanline) {
    if (!mapper || mapper->type != MAPPER_MMC3) return;

    NESMapperMMC3* mmc3 = &mapper->mmc3;

    /* MMC3 IRQ counter decrements on scanlines 0-239 */
    if (scanline >= 0 && scanline < 240) {
        if (mmc3->irq_counter > 0) {
            mmc3->irq_counter--;
        } else {
            mmc3->irq_counter = mmc3->irq_latch;
            if (mmc3->irq_enabled) {
                mmc3->irq_pending = 1;
            }
        }
    }
}

bool nes_mapper_irq_pending(NESMapper* mapper) {
    if (!mapper || mapper->type != MAPPER_MMC3) return false;
    return mapper->mmc3.irq_pending;
}

void nes_mapper_irq_clear(NESMapper* mapper) {
    if (!mapper || mapper->type != MAPPER_MMC3) return;
    mapper->mmc3.irq_pending = 0;
}
