/**
 * @file rom.h
 * @brief NES ROM loader and header parser
 *
 * Handles loading ROM files, parsing the iNES cartridge header,
 * detecting mapper type, and validating ROM structure.
 */

#ifndef RECOMPILER_ROM_H
#define RECOMPILER_ROM_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

namespace gbrecomp {

/* ============================================================================
 * MBC Types (for compatibility with existing code)
 * ========================================================================== */

enum class MBCType : uint8_t {
    NONE = 0x00,
    UNKNOWN = 0xFF
};

/* ============================================================================
 * Mirroring Modes
 * ========================================================================== */

enum class MirroringMode : uint8_t {
    HORIZONTAL = 0,
    VERTICAL = 1,
    FOUR_SCREEN = 2,
    SINGLE_SCREEN_0 = 3,  // Used by some mappers
    SINGLE_SCREEN_1 = 4,  // Used by some mappers
};

/**
 * @brief Get human-readable name for mirroring mode
 */
const char* mirroring_mode_name(MirroringMode mode);

/* ============================================================================
 * ROM Header Structure
 * ========================================================================== */

/**
 * @brief Parsed iNES ROM header information
 *
 * iNES format header is 16 bytes at the start of the ROM file:
 * - Bytes 0-3: "NES\x1A" magic
 * - Byte 4: PRG ROM size (in 16KB units)
 * - Byte 5: CHR ROM size (in 8KB units)
 * - Byte 6: Flags 6 (mapper bits 0-3, mirroring, battery, trainer)
 * - Byte 7: Flags 7 (mapper bits 4-7, VS/Playchoice, NES 2.0)
 * - Bytes 8-15: Reserved/extended (usually zeros for iNES 1.0)
 */
struct NESHeader {
    // Magic (bytes 0-3)
    uint8_t magic[4];              // Should be "NES\x1A"

    // Size fields (bytes 4-5)
    uint8_t prg_rom_size;          // PRG ROM size in 16KB units
    uint8_t chr_rom_size;          // CHR ROM size in 8KB units

    // Flags 6 (byte 6)
    uint8_t flags6;
    bool mirror_mode;              // 0=horizontal, 1=vertical
    bool battery;                  // Battery backup present
    bool trainer;                  // Trainer present (512 bytes before ROM)
    uint8_t mapper_low;            // Lower 4 bits of mapper number

    // Flags 7 (byte 7)
    uint8_t flags7;
    bool vs_system;                // VS UniSystem
    bool playchoice_10;            // PlayChoice-10
    bool nes_2_0;                  // NES 2.0 format
    uint8_t mapper_high;           // Upper 4 bits of mapper number

    // Extended header (bytes 8-15) - NES 2.0 only
    uint8_t prg_ram_size;          // PRG RAM size (NES 2.0)
    uint8_t chr_ram_size;          // CHR RAM size (NES 2.0)
    uint8_t tv_system;             // TV system (NES 2.0)
    uint8_t reserved;              // Reserved (NES 2.0)
    uint8_t extended[8];           // Extended data (NES 2.0)

    // Computed values
    size_t prg_rom_bytes;          // Actual PRG ROM size in bytes
    size_t chr_rom_bytes;          // Actual CHR ROM size in bytes
    uint16_t mapper_number;        // Full mapper number (0-511)
    MirroringMode mirroring;       // Mirroring mode

    // Compatibility fields for existing code (GB-style)
    uint16_t rom_banks;            // Number of 16KB ROM banks (for compatibility)
    MBCType mbc_type;              // MBC type (for compatibility, always NONE for NES)

    // Validation flags
    bool magic_valid;
    bool header_valid;
    bool is_nes_2_0;
};

/* ============================================================================
 * ROM Class
 * ========================================================================== */

/**
 * @brief Loaded ROM with parsed header
 */
class ROM {
public:
    /**
     * @brief Load ROM from file
     * @param path Path to ROM file
     * @return Loaded ROM or nullopt on failure
     */
    static std::optional<ROM> load(const std::filesystem::path& path);

    /**
     * @brief Load ROM from memory buffer
     * @param data ROM data
     * @param name ROM name (for display)
     * @return Loaded ROM or nullopt on failure
     */
    static std::optional<ROM> load_from_buffer(std::vector<uint8_t> data,
                                                const std::string& name);

    // Access ROM data
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    const std::vector<uint8_t>& bytes() const { return data_; }

    // Read at address
    uint8_t read(uint16_t addr) const;
    uint8_t read_banked(uint8_t bank, uint16_t addr) const;

    // Access header
    const NESHeader& header() const { return header_; }

    // ROM info
    const std::string& name() const { return name_; }
    const std::filesystem::path& path() const { return path_; }

    // Validation
    bool is_valid() const { return valid_; }
    const std::string& error() const { return error_; }

    // Helper methods
    bool has_battery() const { return header_.battery; }
    bool has_trainer() const { return header_.trainer; }
    uint16_t mapper() const { return header_.mapper_number; }
    size_t prg_size() const { return header_.prg_rom_bytes; }
    size_t chr_size() const { return header_.chr_rom_bytes; }

private:
    ROM() = default;

    bool parse_header();
    bool validate();

    std::vector<uint8_t> data_;
    NESHeader header_;
    std::string name_;
    std::filesystem::path path_;
    bool valid_ = false;
    std::string error_;
};

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Validate ROM file without fully loading
 */
bool validate_rom_file(const std::filesystem::path& path, std::string& error);

/**
 * @brief Print ROM info to stdout
 */
void print_rom_info(const ROM& rom);

} // namespace gbrecomp

#endif // RECOMPILER_ROM_H
