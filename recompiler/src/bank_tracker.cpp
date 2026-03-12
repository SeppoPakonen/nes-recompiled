/**
 * @file bank_tracker.cpp
 * @brief Bank tracking for NES ROMs
 *
 * For base NES (NROM/mapper 0), there is no bank switching.
 * This file provides infrastructure for future mapper support.
 */

#include "recompiler/bank_tracker.h"

namespace nesrecomp {

/**
 * @brief Record a bank switch at the given address
 *
 * For NROM (mapper 0), this is typically not used.
 * For mappers with bank switching (e.g., MMC1, MMC3), this tracks
 * where bank switches occur in the code.
 *
 * @param addr Address where bank switch occurs (encoded as bank:addr)
 * @param bank Target bank number
 * @param dynamic True if bank value is computed at runtime
 */
void BankTracker::record_bank_switch(uint32_t addr, uint8_t bank, bool dynamic) {
    switches_.push_back({addr, bank, dynamic});
}

/**
 * @brief Record a cross-bank call
 *
 * Tracks calls that cross bank boundaries. For NROM, all calls
 * are within the same physical ROM. For mappers, this helps
 * track which banks call into which other banks.
 *
 * @param from Caller address (encoded as bank:addr)
 * @param to Callee address (encoded as bank:addr)
 * @param from_bank Caller's bank
 * @param to_bank Callee's bank
 */
void BankTracker::record_cross_bank_call(uint32_t from, uint32_t to,
                                          uint8_t from_bank, uint8_t to_bank) {
    calls_.push_back({from, to, from_bank, to_bank});
}

/**
 * @brief Get the active bank at a given address
 *
 * This is a simplified lookup that returns the most recent
 * non-dynamic bank switch before the given address.
 *
 * Note: This is naive and assumes linear execution order,
 * which isn't accurate for branching code. Proper bank
 * tracking requires control-flow awareness.
 *
 * @param addr Address to query
 * @return Bank number, or -1 if unknown
 */
int BankTracker::get_bank_at(uint32_t addr) const {
    int bank = -1; // Unknown by default

    // Find the most recent non-dynamic switch before this address
    for (const auto& sw : switches_) {
        if (sw.addr < addr && !sw.is_dynamic) {
            bank = sw.target_bank;
        }
    }
    return bank;
}

/**
 * @brief Check if there are any dynamic bank switches
 *
 * Dynamic bank switches occur when the bank value is loaded
 * from a register or variable at runtime, making static
 * analysis impossible without additional information.
 *
 * @return True if any dynamic switches exist
 */
bool BankTracker::has_dynamic_switches() const {
    for (const auto& sw : switches_) {
        if (sw.is_dynamic) return true;
    }
    return false;
}

} // namespace nesrecomp
