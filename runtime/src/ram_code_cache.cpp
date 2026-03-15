/**
 * @file ram_code_cache.cpp
 * @brief Implementation of RAM code cache
 */

#include "ram_code_cache.hpp"
#include "nesrt.h"  // For NESContext
#include <cstdio>
#include <cstring>

namespace nesrecomp {

// FNV-1a hash constants
static constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
static constexpr uint64_t FNV_PRIME = 1099511628211ULL;

RAMCodeCache::RAMCodeCache() 
    : RAMCodeCache(DEFAULT_SIZE) {
}

RAMCodeCache::RAMCodeCache(size_t size)
    : size_(size)
    , next_entry_(0)
    , hits_(0)
    , misses_(0)
    , insertions_(0)
    , collisions_(0) {
    // Allocate buckets (power of 2)
    buckets_ = new RAMCodeEntry*[size];
    for (size_t i = 0; i < size; i++) {
        buckets_[i] = nullptr;
    }
    
    // Allocate entry pool
    entries_ = new RAMCodeEntry[size * 4];  // 4x entries for chaining
    for (size_t i = 0; i < size * 4; i++) {
        entries_[i].next = nullptr;
        entries_[i].is_valid = 0;
    }
}

void RAMCodeCache::clear() {
    for (size_t i = 0; i < size_; i++) {
        buckets_[i] = nullptr;
    }
    for (size_t i = 0; i < size_ * 4; i++) {
        entries_[i].next = nullptr;
        entries_[i].is_valid = 0;
    }
    next_entry_ = 0;
    hits_ = 0;
    misses_ = 0;
    insertions_ = 0;
    collisions_ = 0;
}

void (*RAMCodeCache::lookup(uint16_t ram_addr, uint64_t hash))(NESContext*) {
    size_t bucket = get_bucket(hash);
    RAMCodeEntry* entry = buckets_[bucket];
    
    while (entry) {
        if (entry->ram_addr == ram_addr && entry->hash == hash && entry->is_valid) {
            hits_++;
            return entry->compiled_func;
        }
        entry = entry->next;
    }
    
    misses_++;
    return nullptr;
}

bool RAMCodeCache::insert(uint16_t ram_addr, uint64_t hash,
                          void (*func)(NESContext*), uint16_t length) {
    if (next_entry_ >= size_ * 4) {
        return false;  // Cache full
    }
    
    size_t bucket = get_bucket(hash);
    
    // Check if entry already exists
    RAMCodeEntry* entry = buckets_[bucket];
    while (entry) {
        if (entry->ram_addr == ram_addr && entry->hash == hash) {
            // Update existing entry
            entry->compiled_func = func;
            entry->length = length;
            entry->is_valid = 1;
            return true;
        }
        entry = entry->next;
    }
    
    // Create new entry
    RAMCodeEntry* new_entry = &entries_[next_entry_++];
    new_entry->hash = hash;
    new_entry->compiled_func = func;
    new_entry->ram_addr = ram_addr;
    new_entry->length = length;
    new_entry->is_valid = 1;
    new_entry->next = nullptr;
    
    // Add to bucket chain
    if (buckets_[bucket]) {
        collisions_++;
        new_entry->next = buckets_[bucket];
    }
    buckets_[bucket] = new_entry;
    
    insertions_++;
    return true;
}

uint64_t RAMCodeCache::compute_hash(const uint8_t* ram, uint16_t addr, uint16_t length) {
    uint64_t hash = FNV_OFFSET;
    
    for (uint16_t i = 0; i < length; i++) {
        hash ^= ram[addr + i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

RAMCodeCache::Stats RAMCodeCache::get_stats() const {
    Stats stats = {0, 0, 0, 0, 0};
    stats.hits = hits_;
    stats.misses = misses_;
    stats.insertions = insertions_;
    stats.collisions = collisions_;
    stats.entries_used = static_cast<uint32_t>(next_entry_);
    return stats;
}

void RAMCodeCache::print_stats() const {
    Stats stats = get_stats();
    uint32_t total = stats.hits + stats.misses;
    float hit_rate = total > 0 ? (100.0f * stats.hits / total) : 0.0f;
    
    std::fprintf(stderr, "RAM Code Cache Statistics:\n");
    std::fprintf(stderr, "  Hits:       %u\n", stats.hits);
    std::fprintf(stderr, "  Misses:     %u\n", stats.misses);
    std::fprintf(stderr, "  Hit rate:   %.1f%%\n", hit_rate);
    std::fprintf(stderr, "  Insertions: %u\n", stats.insertions);
    std::fprintf(stderr, "  Collisions: %u\n", stats.collisions);
    std::fprintf(stderr, "  Entries:    %u / %zu\n", stats.entries_used, size_ * 4);
}

} // namespace nesrecomp
