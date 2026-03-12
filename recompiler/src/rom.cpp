/**
 * @file rom.cpp
 * @brief NES ROM loader implementation
 */

#include "recompiler/rom.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace gbrecomp {

/* ============================================================================
 * iNES Magic
 * ========================================================================== */

static const uint8_t INES_MAGIC[4] = {'N', 'E', 'S', 0x1A};
static const uint8_t NES2_MAGIC[4] = {'N', 'E', 'S', 0x1B};

/* ============================================================================
 * Mirroring Mode Helpers
 * ========================================================================== */

const char* mirroring_mode_name(MirroringMode mode) {
    switch (mode) {
        case MirroringMode::HORIZONTAL: return "Horizontal";
        case MirroringMode::VERTICAL: return "Vertical";
        case MirroringMode::FOUR_SCREEN: return "Four-screen";
        case MirroringMode::SINGLE_SCREEN_0: return "Single-screen 0";
        case MirroringMode::SINGLE_SCREEN_1: return "Single-screen 1";
        default: return "Unknown";
    }
}

/* ============================================================================
 * ROM Implementation
 * ========================================================================== */

std::optional<ROM> ROM::load(const std::filesystem::path& path) {
    ROM rom;
    rom.path_ = path;
    rom.name_ = path.stem().string();

    // Open file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        rom.error_ = "Failed to open file";
        return rom;
    }

    // Get file size
    auto file_size = file.tellg();
    if (file_size < 16) {
        rom.error_ = "File too small to be a valid ROM";
        return rom;
    }

    // Read file
    rom.data_.resize(static_cast<size_t>(file_size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(rom.data_.data()), file_size);

    if (!file) {
        rom.error_ = "Failed to read file";
        return rom;
    }

    // Parse header
    if (!rom.parse_header()) {
        return rom;
    }

    // Validate
    if (!rom.validate()) {
        return rom;
    }

    rom.valid_ = true;
    return rom;
}

std::optional<ROM> ROM::load_from_buffer(std::vector<uint8_t> data,
                                          const std::string& name) {
    ROM rom;
    rom.data_ = std::move(data);
    rom.name_ = name;

    if (rom.data_.size() < 16) {
        rom.error_ = "Data too small to be a valid ROM";
        return rom;
    }

    if (!rom.parse_header()) {
        return rom;
    }

    if (!rom.validate()) {
        return rom;
    }

    rom.valid_ = true;
    return rom;
}

bool ROM::parse_header() {
    // Zero-initialize header
    header_ = NESHeader{};

    // Check magic (bytes 0-3)
    std::copy_n(data_.data(), 4, header_.magic);
    
    header_.magic_valid = (std::memcmp(header_.magic, INES_MAGIC, 4) == 0);
    header_.is_nes_2_0 = (std::memcmp(header_.magic, NES2_MAGIC, 4) == 0);

    if (!header_.magic_valid && !header_.is_nes_2_0) {
        error_ = "Invalid iNES magic (expected 'NES\\x1A' or 'NES\\x1B')";
        return false;
    }

    // PRG ROM size (byte 4)
    header_.prg_rom_size = data_[4];
    header_.prg_rom_bytes = static_cast<size_t>(header_.prg_rom_size) * 16 * 1024;

    // CHR ROM size (byte 5)
    header_.chr_rom_size = data_[5];
    header_.chr_rom_bytes = static_cast<size_t>(header_.chr_rom_size) * 8 * 1024;

    // Flags 6 (byte 6)
    header_.flags6 = data_[6];
    header_.mirror_mode = (header_.flags6 & 0x01) != 0;
    header_.battery = (header_.flags6 & 0x02) != 0;
    header_.trainer = (header_.flags6 & 0x04) != 0;
    header_.mapper_low = (header_.flags6 >> 4) & 0x0F;

    // Flags 7 (byte 7)
    header_.flags7 = data_[7];
    header_.vs_system = (header_.flags7 & 0x01) != 0;
    header_.playchoice_10 = (header_.flags7 & 0x02) != 0;
    header_.nes_2_0 = (header_.flags7 & 0x0C) == 0x0C;
    header_.mapper_high = (header_.flags7 >> 4) & 0x0F;

    // Calculate mapper number
    header_.mapper_number = (static_cast<uint16_t>(header_.mapper_high) << 4) | 
                            static_cast<uint16_t>(header_.mapper_low);

    // Set compatibility fields
    header_.rom_banks = static_cast<uint16_t>(header_.prg_rom_bytes / (16 * 1024));
    header_.mbc_type = MBCType::NONE;  // NES uses mappers, not MBC

    // Determine mirroring mode
    // For NES 2.0, bit 3 of flags 6 is used for four-screen VRAM
    if (header_.is_nes_2_0) {
        if (header_.flags6 & 0x08) {
            header_.mirroring = MirroringMode::FOUR_SCREEN;
        } else {
            header_.mirroring = header_.mirror_mode ? 
                MirroringMode::VERTICAL : MirroringMode::HORIZONTAL;
        }
    } else {
        // iNES 1.0 - check for four-screen from mapper
        // Mapper 35 (MMC5) and some others use four-screen
        // For now, use simple logic
        if (header_.mapper_number == 35 || header_.mapper_number == 80) {
            header_.mirroring = MirroringMode::FOUR_SCREEN;
        } else {
            header_.mirroring = header_.mirror_mode ? 
                MirroringMode::VERTICAL : MirroringMode::HORIZONTAL;
        }
    }

    // Parse NES 2.0 extended header if applicable
    if (header_.is_nes_2_0) {
        header_.prg_ram_size = data_[8];
        header_.chr_ram_size = data_[9];
        header_.tv_system = data_[10];
        header_.reserved = data_[11];
        std::copy_n(data_.data() + 8, 8, header_.extended);
    } else {
        // iNES 1.0 - reserved bytes should be zero
        header_.prg_ram_size = 0;
        header_.chr_ram_size = 0;
        header_.tv_system = 0;
        header_.reserved = 0;
        std::fill_n(header_.extended, 8, 0);
    }

    return true;
}

bool ROM::validate() {
    header_.header_valid = true;

    // Validate magic
    if (!header_.magic_valid && !header_.is_nes_2_0) {
        error_ = "Invalid iNES magic";
        header_.header_valid = false;
        return false;
    }

    // Check for NES 2.0 flag consistency
    if (header_.nes_2_0 && !header_.is_nes_2_0) {
        // NES 2.0 flag set but magic is not NES 2.0
        // This is an invalid header
        error_ = "Invalid NES 2.0 flag with iNES 1.0 magic";
        header_.header_valid = false;
        return false;
    }

    // Validate PRG ROM size (must be at least 16KB for most games)
    if (header_.prg_rom_bytes == 0) {
        error_ = "Invalid PRG ROM size (must be at least 16KB)";
        header_.header_valid = false;
        return false;
    }

    // Check file size matches header (accounting for trainer if present)
    size_t expected_size = header_.prg_rom_bytes + header_.chr_rom_bytes;
    if (header_.trainer) {
        expected_size += 512;  // Trainer is 512 bytes
    }

    if (data_.size() < expected_size) {
        error_ = "ROM file smaller than header indicates";
        header_.header_valid = false;
        return false;
    }

    // For iNES 1.0, check that reserved bytes are zero (common validation)
    if (!header_.is_nes_2_0) {
        for (int i = 8; i < 16; i++) {
            if (data_[i] != 0) {
                // Some ROMs have non-zero reserved bytes, just warn
                std::cerr << "[WARN] Reserved header byte " << i << " is non-zero\n";
            }
        }
    }

    return true;
}

uint8_t ROM::read(uint16_t addr) const {
    if (addr < data_.size()) {
        return data_[addr];
    }
    return 0xFF;
}

uint8_t ROM::read_banked(uint8_t bank, uint16_t addr) const {
    if (bank == 0 || addr < 0x4000) {
        // Bank 0 or fixed bank area
        if (addr < data_.size()) {
            return data_[addr];
        }
    } else {
        // Switchable bank area (0x4000-0x7FFF)
        size_t offset = (static_cast<size_t>(bank) * 0x4000) + (addr - 0x4000);
        if (offset < data_.size()) {
            return data_[offset];
        }
    }
    return 0xFF;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

bool validate_rom_file(const std::filesystem::path& path, std::string& error) {
    auto rom = ROM::load(path);
    if (!rom || !rom->is_valid()) {
        error = rom ? rom->error() : "Failed to load ROM";
        return false;
    }
    return true;
}

void print_rom_info(const ROM& rom) {
    const auto& h = rom.header();

    std::cout << "\nROM Information:\n";
    std::cout << "  Format:       " << (h.is_nes_2_0 ? "NES 2.0" : "iNES 1.0") << "\n";
    std::cout << "  PRG ROM:      " << (h.prg_rom_bytes / 1024) << " KB ("
              << (int)h.prg_rom_size << " x 16KB)\n";
    std::cout << "  CHR ROM:      " << (h.chr_rom_bytes / 1024) << " KB ("
              << (int)h.chr_rom_size << " x 8KB)\n";
    std::cout << "  Mapper:       " << h.mapper_number << "\n";
    std::cout << "  Mirroring:    " << mirroring_mode_name(h.mirroring) << "\n";
    
    if (h.battery) {
        std::cout << "  Battery:      Yes\n";
    }
    
    if (h.trainer) {
        std::cout << "  Trainer:      Yes (512 bytes)\n";
    }
    
    if (h.vs_system) {
        std::cout << "  VS System:    Yes\n";
    }
    
    if (h.playchoice_10) {
        std::cout << "  PlayChoice-10: Yes\n";
    }

    if (h.is_nes_2_0) {
        std::cout << "  PRG RAM:      " << (int)h.prg_ram_size << " units\n";
        std::cout << "  CHR RAM:      " << (int)h.chr_ram_size << " units\n";
        std::cout << "  TV System:    " << (int)h.tv_system << "\n";
    }

    std::cout << "  Magic Valid:  " << (h.magic_valid ? "Yes" : "No") << "\n";
    std::cout << "  Header Valid: " << (h.header_valid ? "Yes" : "No") << "\n";
}

} // namespace gbrecomp
