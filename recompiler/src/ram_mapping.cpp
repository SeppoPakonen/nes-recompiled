/**
 * @file ram_mapping.cpp
 * @brief Implementation of RAM code mapping
 */

#include "recompiler/ram_mapping.h"

namespace nesrecomp {

void RAMMappingTable::add_mapping(const RAMCodeMapping& mapping) {
    by_ram_addr_[mapping.ram_addr] = mapping;
    all_mappings_.push_back(mapping);
}

bool RAMMappingTable::has_mapping(uint16_t ram_addr) const {
    return by_ram_addr_.find(ram_addr) != by_ram_addr_.end();
}

const RAMCodeMapping* RAMMappingTable::get_mapping(uint16_t ram_addr) const {
    auto it = by_ram_addr_.find(ram_addr);
    if (it != by_ram_addr_.end()) {
        return &it->second;
    }
    return nullptr;
}

void RAMMappingTable::clear() {
    by_ram_addr_.clear();
    all_mappings_.clear();
}

} // namespace nesrecomp
