/**
 * @file ram_mapping.h
 * @brief RAM code mapping for NES recompiler
 * 
 * Maps RAM addresses to their ROM sources for pre-compilation.
 */

#ifndef RECOMPILER_RAM_MAPPING_H
#define RECOMPILER_RAM_MAPPING_H

#include <cstdint>
#include <map>
#include <vector>
#include <string>

namespace nesrecomp {

/**
 * @brief A mapping from RAM destination to ROM source
 * 
 * Used when games copy code from ROM to RAM and execute it.
 */
struct RAMCodeMapping {
    uint16_t ram_addr;      ///< RAM destination address (0x0000-0x07FF)
    uint16_t rom_addr;      ///< ROM source address
    uint8_t rom_bank;       ///< ROM bank
    uint16_t length;        ///< Bytes copied
    bool verified;          ///< Destination is executed (has JMP/JSR)
    
    bool operator<(const RAMCodeMapping& other) const {
        return ram_addr < other.ram_addr;
    }
};

/**
 * @brief Copy pattern detected in code
 * 
 * Represents a sequence like:
 *   LDY #$00
 *   loop:
 *     LDA $8000,Y
 *     STA $0600,Y
 *     DEY
 *     BNE loop
 */
struct CopyPattern {
    uint16_t src_start;     ///< ROM source start address
    uint16_t dst_start;     ///< RAM destination start address
    uint16_t length;        ///< Bytes copied
    uint8_t src_bank;       ///< ROM bank
    uint16_t loop_start;    ///< Address of loop start
    uint16_t loop_end;      ///< Address of loop end (BNE)
    uint16_t jump_target;   ///< JMP/JSR target after copy (0 if none)
    bool verified;          ///< Jump target matches destination
    
    bool operator<(const CopyPattern& other) const {
        return dst_start < other.dst_start;
    }
};

/**
 * @brief Table of RAM code mappings
 * 
 * Provides O(log n) lookup by RAM address.
 */
class RAMMappingTable {
public:
    /**
     * @brief Add a mapping to the table
     */
    void add_mapping(const RAMCodeMapping& mapping);
    
    /**
     * @brief Check if a RAM address has a mapping
     */
    bool has_mapping(uint16_t ram_addr) const;
    
    /**
     * @brief Get mapping for a RAM address
     * @return Pointer to mapping, or nullptr if not found
     */
    const RAMCodeMapping* get_mapping(uint16_t ram_addr) const;
    
    /**
     * @brief Get all mappings
     */
    const std::vector<RAMCodeMapping>& mappings() const {
        return all_mappings_;
    }
    
    /**
     * @brief Get number of mappings
     */
    size_t size() const {
        return all_mappings_.size();
    }
    
    /**
     * @brief Clear all mappings
     */
    void clear();
    
private:
    std::map<uint16_t, RAMCodeMapping> by_ram_addr_;  ///< Lookup by RAM address
    std::vector<RAMCodeMapping> all_mappings_;        ///< All mappings
};

} // namespace nesrecomp

#endif // RECOMPILER_RAM_MAPPING_H
