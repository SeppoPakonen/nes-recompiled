/**
 * @file ram_code_cache.hpp
 * @brief Runtime cache for dynamically recompiled RAM code
 * 
 * Provides hash-based caching for RAM code that's copied from ROM
 * or generated at runtime. Uses FNV-1a hash for fast lookup.
 */

#ifndef RAM_CODE_CACHE_HPP
#define RAM_CODE_CACHE_HPP

#include <cstdint>
#include <cstddef>

namespace nesrecomp {

/**
 * @brief Cache entry for compiled RAM code
 */
struct RAMCodeEntry {
    uint64_t hash;                          ///< FNV-1a hash of RAM contents
    void (*compiled_func)(struct NESContext*);  ///< Compiled function pointer
    uint16_t ram_addr;                      ///< RAM address (0x0000-0x07FF)
    uint16_t length;                        ///< Code length in bytes
    uint8_t is_valid;                       ///< Entry is valid (1) or empty (0)
    RAMCodeEntry* next;                     ///< Hash collision chain
};

/**
 * @brief Hash table for RAM code cache
 * 
 * Uses open addressing with chaining for collision resolution.
 * Default size is 256 entries (2^8).
 */
class RAMCodeCache {
public:
    static constexpr size_t DEFAULT_SIZE = 256;
    static constexpr size_t HASH_BITS = 8;
    
    /**
     * @brief Initialize cache with default size
     */
    RAMCodeCache();
    
    /**
     * @brief Initialize cache with custom size
     * @param size Number of buckets (should be power of 2)
     */
    explicit RAMCodeCache(size_t size);
    
    /**
     * @brief Clear all entries
     */
    void clear();
    
    /**
     * @brief Lookup compiled code by RAM address and hash
     * @param ram_addr RAM address to lookup
     * @param hash Hash of RAM contents
     * @return Compiled function pointer, or nullptr if not found
     */
    void (*lookup(uint16_t ram_addr, uint64_t hash))(struct NESContext*);
    
    /**
     * @brief Insert compiled code into cache
     * @param ram_addr RAM address
     * @param hash Hash of RAM contents
     * @param func Compiled function pointer
     * @param length Code length
     * @return true if inserted, false if cache is full
     */
    bool insert(uint16_t ram_addr, uint64_t hash, 
                void (*func)(struct NESContext*), uint16_t length);
    
    /**
     * @brief Compute FNV-1a hash of RAM contents
     * @param ram Pointer to RAM
     * @param addr Start address
     * @param length Number of bytes to hash
     * @return FNV-1a hash
     */
    static uint64_t compute_hash(const uint8_t* ram, uint16_t addr, uint16_t length);
    
    /**
     * @brief Get cache statistics
     */
    struct Stats {
        uint32_t hits;
        uint32_t misses;
        uint32_t insertions;
        uint32_t collisions;
        uint32_t entries_used;
    };
    
    Stats get_stats() const;
    
    /**
     * @brief Print cache statistics to stderr
     */
    void print_stats() const;
    
private:
    RAMCodeEntry** buckets_;  ///< Array of bucket pointers
    RAMCodeEntry* entries_;   ///< Entry pool
    size_t size_;
    size_t next_entry_;
    
    mutable uint32_t hits_;
    mutable uint32_t misses_;
    uint32_t insertions_;
    uint32_t collisions_;
    
    /**
     * @brief Get bucket index for hash
     */
    size_t get_bucket(uint64_t hash) const {
        return hash & (size_ - 1);
    }
};

} // namespace nesrecomp

#endif // RECOMPILER_RAM_CODE_CACHE_H
