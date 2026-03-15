/**
 * @file analyzer.h
 * @brief Control flow analysis for 6502 (NES) ROMs
 */

#ifndef RECOMPILER_ANALYZER_H
#define RECOMPILER_ANALYZER_H

#include "decoder_6502.h"
#include "rom.h"
#include "bank_tracker.h"
#include "ram_mapping.h"
#include <map>
#include <set>
#include <vector>
#include <string>

namespace nesrecomp {

// NES interrupt vector addresses
constexpr uint16_t NES_VECTOR_NMI = 0xFFFA;   ///< NMI vector
constexpr uint16_t NES_VECTOR_RESET = 0xFFFC; ///< Reset vector
constexpr uint16_t NES_VECTOR_IRQ = 0xFFFE;   ///< IRQ/BRK vector

/* ============================================================================
 * Basic Block
 * ========================================================================== */

/**
 * @brief A basic block - sequence of instructions without branches
 */
struct BasicBlock {
    uint16_t start_address;
    uint16_t end_address;
    uint8_t bank;                       ///< ROM bank this block belongs to

    std::vector<size_t> instruction_indices;

    // Control flow
    std::vector<uint16_t> successors;   ///< Addresses of successor blocks
    std::vector<uint16_t> predecessors; ///< Addresses of predecessor blocks

    // Cross-bank info (for future mapper support)
    bool has_cross_bank_successor = false;

    // Labels needed within this block
    std::set<uint16_t> internal_labels;

    // Is this block the entry to a function?
    bool is_function_entry = false;

    // Is this block an interrupt handler entry?
    bool is_interrupt_entry = false;

    // Is this block reachable from entry points?
    bool is_reachable = false;
};

/* ============================================================================
 * Function
 * ========================================================================== */

/**
 * @brief A function - collection of basic blocks with single entry
 */
struct Function {
    std::string name;
    uint16_t entry_address;
    uint8_t bank;

    std::vector<uint16_t> block_addresses;

    bool is_interrupt_handler = false;
    bool crosses_banks = false;         ///< Calls into other banks (for mappers)
    bool is_called_cross_bank = false;  ///< Called from other banks
};

/* ============================================================================
 * Analysis Result
 * ========================================================================== */

/**
 * @brief Complete control flow analysis result
 */
struct AnalysisResult {
    // ROM reference
    const ROM* rom = nullptr;

    // All decoded instructions
    std::vector<Instruction6502> instructions;

    // Address to instruction index map
    std::map<uint32_t, size_t> addr_to_index;  ///< (bank << 16 | addr) -> index

    // Basic blocks indexed by (bank << 16 | addr)
    std::map<uint32_t, BasicBlock> blocks;

    // Functions indexed by (bank << 16 | addr)
    std::map<uint32_t, Function> functions;

    // Labels needed (jump targets)
    std::set<uint32_t> label_addresses;  ///< (bank << 16 | addr)

    // Call targets (function entry points)
    std::set<uint32_t> call_targets;

    // Computed jump targets (for indirect JMP)
    std::set<uint32_t> computed_jump_targets;

    // Bank switch points (for future mapper support)
    std::set<uint16_t> bank_switch_addresses;

    // RAM code mappings (for games that copy code to RAM)
    RAMMappingTable ram_mappings;

    // Copy patterns detected (for debugging)
    std::vector<CopyPattern> copy_patterns;

    // Entry point (reset vector)
    uint16_t entry_point = NES_VECTOR_RESET;

    // Interrupt vectors (NMI, Reset, IRQ)
    std::vector<uint16_t> interrupt_vectors;

    // Statistics
    struct {
        size_t total_instructions = 0;
        size_t total_blocks = 0;
        size_t total_functions = 0;
        size_t unreachable_instructions = 0;
        size_t cross_bank_calls = 0;
    } stats;

    // Bank tracker results (for future mapper support)
    BankTracker bank_tracker;

    // Helper to create combined address
    static uint32_t make_addr(uint8_t bank, uint16_t addr) {
        return (static_cast<uint32_t>(bank) << 16) | addr;
    }

    // Get instruction at bank:addr
    const Instruction6502* get_instruction(uint8_t bank, uint16_t addr) const;

    // Get block at bank:addr
    const BasicBlock* get_block(uint8_t bank, uint16_t addr) const;

    // Get function at bank:addr
    const Function* get_function(uint8_t bank, uint16_t addr) const;
};

/* ============================================================================
 * Analyzer Interface
 * ========================================================================== */

/**
 * @brief Analysis options
 */
struct AnalyzerOptions {
    // Map of RAM address -> ROM address (source of the code)
    // This allows analyzing code that is copied to RAM (e.g. NMI handlers)
    struct RamOverlay {
        uint16_t ram_addr;
        uint32_t rom_addr;
        uint16_t size;
    };
    std::vector<RamOverlay> ram_overlays;

    // Explicit list of entry points to analyze (in addition to standard vectors)
    std::vector<uint32_t> entry_points;

    bool analyze_all_banks = true;      ///< Analyze all ROM banks (for mappers)
    bool detect_computed_jumps = true;  ///< Try to resolve indirect JMP targets
    bool track_bank_switches = true;    ///< Track bank switch operations (mappers)
    bool mark_unreachable = true;       ///< Mark unreachable code

    // Debugging options
    bool trace_log = false;             ///< Print detailed execution trace
    bool verbose = false;               ///< Print verbose analysis info
    size_t max_instructions = 0;        ///< Max instructions to analyze (0 = infinite)
    size_t max_functions = 0;           ///< Max functions to discover (0 = infinite)

    // Feature flags
    bool aggressive_scan = true;        ///< Scan for unreferenced code (ON by default)
    std::string trace_file_path;        ///< Path to entry points trace file

    // NES-specific options
    bool follow_nmi = true;             ///< Follow NMI handler at $FFFA
    bool follow_irq = true;             ///< Follow IRQ handler at $FFFE
};

/* ============================================================================
 * Copy Pattern Detection
 * ========================================================================== */

/**
 * @brief Detect ROM→RAM copy patterns in analyzed code
 * 
 * Scans for instruction sequences that copy code from ROM to RAM:
 *   LDY #$LL
 *   loop:
 *     LDA $XXXX,Y    ; Read from ROM
 *     STA $YYYY,Y    ; Write to RAM
 *     DEY / INY
 *     BNE loop
 *     JMP $YYYY      ; Jump to copied code
 * 
 * @param result Analysis result with decoded instructions
 * @return Vector of detected copy patterns
 */
std::vector<CopyPattern> detect_copy_patterns(const AnalysisResult& result);

/**
 * @brief Analyze a ROM
 * 
 * @param rom Loaded ROM
 * @param options Analysis options
 * @return Analysis result
 */
AnalysisResult analyze(const ROM& rom, const AnalyzerOptions& options = {});

/**
 * @brief Analyze a single bank
 * 
 * @param rom Loaded ROM
 * @param bank Bank number to analyze
 * @param options Analysis options
 * @return Partial analysis for that bank
 */
AnalysisResult analyze_bank(const ROM& rom, uint8_t bank,
                            const AnalyzerOptions& options = {});

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Generate function name for an address
 */
std::string generate_function_name(uint8_t bank, uint16_t address);

/**
 * @brief Generate label name for an address
 */
std::string generate_label_name(uint8_t bank, uint16_t address);

/**
 * @brief Print analysis summary
 */
void print_analysis_summary(const AnalysisResult& result);

/**
 * @brief Check if address is likely data (not code)
 */
bool is_likely_data(const AnalysisResult& result, uint8_t bank, uint16_t address);

} // namespace nesrecomp

#endif // RECOMPILER_ANALYZER_H
