/**
 * @file analyzer.cpp
 * @brief 6502 control flow analyzer implementation
 */

#include "recompiler/analyzer.h"
#include <algorithm>
#include <queue>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <map>
#include <fstream>

namespace nesrecomp {

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

static uint32_t make_address(uint8_t bank, uint16_t addr) {
    return (static_cast<uint32_t>(bank) << 16) | addr;
}

static uint8_t get_bank(uint32_t addr) {
    return static_cast<uint8_t>(addr >> 16);
}

static uint16_t get_offset(uint32_t addr) {
    return static_cast<uint16_t>(addr & 0xFFFF);
}

/* ============================================================================
 * AnalysisResult Implementation
 * ========================================================================== */

const Instruction6502* AnalysisResult::get_instruction(uint8_t bank, uint16_t addr) const {
    uint32_t full_addr = make_addr(bank, addr);
    auto it = addr_to_index.find(full_addr);
    if (it != addr_to_index.end() && it->second < instructions.size()) {
        return &instructions[it->second];
    }
    return nullptr;
}

const BasicBlock* AnalysisResult::get_block(uint8_t bank, uint16_t addr) const {
    uint32_t full_addr = make_addr(bank, addr);
    auto it = blocks.find(full_addr);
    if (it != blocks.end()) {
        return &it->second;
    }
    return nullptr;
}

const Function* AnalysisResult::get_function(uint8_t bank, uint16_t addr) const {
    uint32_t full_addr = make_addr(bank, addr);
    auto it = functions.find(full_addr);
    if (it != functions.end()) {
        return &it->second;
    }
    return nullptr;
}

/* ============================================================================
 * Internal State Tracking
 * ========================================================================== */

// Track addresses to explore
struct AnalysisState {
    uint32_t addr;
    uint8_t current_bank;
};

/* ============================================================================
 * Analysis Implementation
 * ========================================================================== */

/**
 * @brief Load entry points from a runtime trace file
 */
static void load_trace_entry_points(const std::string& path, std::set<uint32_t>& call_targets) {
    if (path.empty()) return;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open trace file: " << path << "\n";
        return;
    }

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            try {
                int bank = std::stoi(line.substr(0, colon));
                int addr = std::stoi(line.substr(colon + 1), nullptr, 16);
                call_targets.insert(make_address(bank, addr));
                count++;
            } catch (...) {
                continue;
            }
        }
    }
    std::cout << "Loaded " << count << " entry points from trace file: " << path << "\n";
}

/**
 * @brief Check if an address looks like valid code start (heuristic)
 */
static bool is_likely_valid_code(const ROM& rom, uint8_t bank, uint16_t addr) {
    // Basic check: ensure we can read from this address
    if (addr >= 0x8000) return false;

    // Check for obvious padding
    uint8_t b0 = rom.read_banked(bank, addr);
    if (b0 == 0x00 || b0 == 0xFF) return false;

    // Decode first instruction
    Decoder6502 decoder(rom);
    Instruction6502 instr = decoder.decode(addr, bank);

    // Check if it's a valid instruction
    if (instr.opcode_type == Opcode::UNDEFINED ||
        instr.opcode_type == Opcode::NIL_OP) {
        return false;
    }

    return true;
}

AnalysisResult analyze(const ROM& rom, const AnalyzerOptions& options) {
    AnalysisResult result;
    result.rom = &rom;
    result.entry_point = NES_VECTOR_RESET;

    // Add NES interrupt vectors
    result.interrupt_vectors = {NES_VECTOR_NMI, NES_VECTOR_RESET, NES_VECTOR_IRQ};

    Decoder6502 decoder(rom);

    std::queue<AnalysisState> work_queue;
    std::set<uint32_t> visited;

    // Load from trace if provided
    load_trace_entry_points(options.trace_file_path, result.call_targets);

    // Add standard NES entry points
    // Reset vector is always the main entry point
    result.call_targets.insert(make_address(0, NES_VECTOR_RESET));

    // NMI vector (optional)
    if (options.follow_nmi) {
        result.call_targets.insert(make_address(0, NES_VECTOR_NMI));
    }

    // IRQ/BRK vector (optional)
    if (options.follow_irq) {
        result.call_targets.insert(make_address(0, NES_VECTOR_IRQ));
    }

    // Add manual entry points
    for (uint32_t addr : options.entry_points) {
        result.call_targets.insert(addr);
    }

    // Initial work queue seeding
    for (uint32_t target : result.call_targets) {
        uint8_t bank = get_bank(target);
        work_queue.push({target, (bank > 0 ? bank : (uint8_t)0)});
    }

    // For ROMs with mappers, analyze all banks if requested
    if (rom.header().mapper_number > 0 && options.analyze_all_banks) {
        // For mapper 0 (NROM), there's only one bank
        // For other mappers, we may have multiple banks
        uint16_t num_banks = rom.header().rom_banks;
        for (uint16_t b = 1; b < num_banks && b < 256; b++) {
            uint8_t bank = static_cast<uint8_t>(b);
            // Check if bank start looks like code
            if (is_likely_valid_code(rom, bank, 0x4000)) {
                work_queue.push({make_address(bank, 0x4000), bank});
                result.call_targets.insert(make_address(bank, 0x4000));
            }
        }
    }

    // Add overlay entry points
    for (const auto& ov : options.ram_overlays) {
        uint32_t addr = make_address(0, ov.ram_addr);
        result.call_targets.insert(addr);
        work_queue.push({addr, 0});
    }

    // Multi-pass analysis
    bool scanning_pass = false;

    // Explore all reachable code
    while (true) {
        // Drain work queue
        while (!work_queue.empty()) {
            auto item = work_queue.front();
            work_queue.pop();

            uint32_t addr = item.addr;
            uint8_t current_bank = item.current_bank;

            if (visited.count(addr)) continue;

            uint8_t bank = get_bank(addr);
            uint16_t offset = get_offset(addr);

            // Only analyze ROM space
            if (offset >= 0x8000) continue;

            // Bank mapping rules for NES
            // Bank 0: $0000-$7FFF (or $0000-$3FFF for first 16KB, mirrored)
            // For NROM (mapper 0): PRG ROM is 16KB or 32KB
            // - 16KB: $8000-$FFFF maps to single bank
            // - 32KB: $8000-$BFFF and $C000-$FFFF are two banks
            if (offset < 0x4000) {
                // Fixed region - always bank 0
                bank = 0;
                addr = make_address(0, offset);
                if (visited.count(addr)) continue;
            } else if (offset < 0x8000) {
                // Switchable region - use current bank context
                if (bank == 0) {
                    // Default to bank 1 for addresses in switchable region
                    bank = 1;
                    addr = make_address(1, offset);
                    if (visited.count(addr)) continue;
                }
                current_bank = bank;
            }

            // Calculate ROM offset
            size_t rom_offset;
            if (offset < 0x4000) {
                rom_offset = offset;
            } else {
                // For switchable region, calculate based on bank
                // Each bank is 16KB (0x4000 bytes)
                rom_offset = static_cast<size_t>(bank) * 0x4000 + (offset - 0x4000);
            }

            if (rom_offset >= rom.size()) continue;

            visited.insert(addr);

            // Decode instruction
            Instruction6502 instr = decoder.decode(offset, bank);

            // Trace logging
            if (options.trace_log) {
                std::cout << "[TRACE] " << std::hex << std::setfill('0')
                          << std::setw(2) << (int)bank << ":"
                          << std::setw(4) << offset << " "
                          << instr.disassemble() << std::dec << "\n";
            }

            // Check for padding (common in ROMs)
            if (bank > 0 && instr.opcode == 0xFF) {
                bool is_padding = true;
                for (int i = 1; i < 16; i++) {
                    if (rom.read_banked(bank, offset + i) != 0xFF) {
                        is_padding = false;
                        break;
                    }
                }
                if (is_padding) continue;
            }

            if (instr.opcode_type == Opcode::UNDEFINED) {
                std::cout << "[ERROR] Undefined instruction at "
                          << std::hex << (int)bank << ":" << offset << "\n";
                continue;
            }

            if (options.max_instructions > 0 &&
                result.instructions.size() >= options.max_instructions) {
                break;
            }

            size_t idx = result.instructions.size();
            result.instructions.push_back(instr);
            result.addr_to_index[addr] = idx;

            // Helper to determine target bank for jumps/calls
            auto target_bank = [&](uint16_t target) -> uint8_t {
                if (target < 0x4000) return 0;
                // For NROM (mapper 0), all addresses >= 0x4000 are in the same bank context
                if (rom.header().mapper_number == 0) return current_bank > 0 ? current_bank : 1;
                return current_bank;
            };

            /* ========================================================================
             * Control Flow Handling
             * ====================================================================== */

            if (instr.is_call) {
                // JSR - Jump to Subroutine
                uint16_t target = instr.imm16;
                uint8_t tbank = target_bank(target);
                instr.resolved_target_bank = tbank;

                // Validate target
                if (tbank > 0 && tbank != bank) {
                    if (!is_likely_valid_code(rom, tbank, target)) {
                        // Skip invalid targets
                    }
                }

                result.call_targets.insert(make_address(tbank, target));
                work_queue.push({make_address(tbank, target), tbank});

                // Track cross-bank calls (for mappers)
                if (tbank != bank) {
                    result.stats.cross_bank_calls++;
                    result.bank_tracker.record_cross_bank_call(
                        addr, make_address(tbank, target), bank, tbank);
                }

                // Fall through to next instruction
                uint32_t fall_through = make_address(bank, offset + instr.length);
                result.label_addresses.insert(fall_through);
                work_queue.push({fall_through, current_bank});

            } else if (instr.is_jump) {
                if (instr.mode == AddressMode::ABS ||
                    instr.mode == AddressMode::ABS_X ||
                    instr.mode == AddressMode::ABS_Y) {
                    // JMP absolute - direct jump to address
                    uint16_t target = instr.imm16;
                    uint8_t tbank = target_bank(target);
                    instr.resolved_target_bank = tbank;

                    if (target >= 0x4000 && target <= 0x7FFF) {
                        if (tbank > 0 && tbank != bank) {
                            if (!is_likely_valid_code(rom, tbank, target)) {
                                // Skip invalid
                            }
                        }
                        result.call_targets.insert(make_address(tbank, target));
                    }
                    result.label_addresses.insert(make_address(tbank, target));
                    work_queue.push({make_address(tbank, target), tbank});

                } else if (instr.mode == AddressMode::IND) {
                    // JMP indirect - jump to address stored at (imm16)
                    // We can't statically resolve this without runtime info
                    // But we can record the pointer location
                    result.computed_jump_targets.insert(make_address(bank, instr.imm16));

                } else if (instr.mode == AddressMode::REL) {
                    // Branch instructions (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS)
                    // Relative offset is signed 8-bit
                    uint16_t target = offset + instr.length + instr.offset;
                    result.label_addresses.insert(make_address(bank, target));
                    work_queue.push({make_address(bank, target), current_bank});
                }

                // Conditional jumps also fall through
                if (instr.is_conditional) {
                    uint32_t fall_through = make_address(bank, offset + instr.length);
                    result.label_addresses.insert(fall_through);
                    work_queue.push({fall_through, current_bank});
                }

            } else if (instr.is_return) {
                // RTS or RTI - return from subroutine/interrupt
                // No fall-through, this is a terminator

            } else if (instr.opcode_type == Opcode::BRK) {
                // BRK - software interrupt
                // Typically followed by a padding byte, then code continues
                // For analysis, treat as terminator
            } else {
                // Normal instruction - continue to next
                work_queue.push({make_address(bank, offset + instr.length), current_bank});
            }
        } // End work queue loop

        // Aggressive Code Scanning (optional)
        if (options.aggressive_scan && !scanning_pass) {
            scanning_pass = true;

            if (options.verbose) {
                std::cout << "[ANALYSIS] Starting aggressive scan for missing code..." << std::endl;
            }

            size_t found_count = 0;
            static std::set<uint32_t> aggressive_regions;

            // Scan bank 0
            for (uint32_t addr = 0x0100; addr < 0x3FFE; addr++) {
                uint32_t full_addr = make_address(0, addr);

                if (visited.count(full_addr) || aggressive_regions.count(full_addr)) {
                    continue;
                }

                // Skip padding
                uint8_t byte = rom.read_banked(0, addr);
                if (byte == 0xFF || byte == 0x00) {
                    continue;
                }

                // Check if this looks like valid code
                if (is_likely_valid_code(rom, 0, addr)) {
                    if (options.verbose) {
                        std::cout << "[ANALYSIS] Detected potential function at "
                                  << std::hex << "0:" << addr << std::dec << "\n";
                    }

                    result.call_targets.insert(full_addr);
                    work_queue.push({full_addr, 0});
                    found_count++;
                }
            }

            if (found_count > 0 && options.verbose) {
                std::cout << "[ANALYSIS] Found " << found_count
                          << " new entry points. Restarting analysis." << std::endl;
            }
        }

        // If we get here, we are done
        break;
    } // End while(true)

    // ============================================================================
    // Build Basic Blocks
    // ============================================================================

    std::set<uint32_t> block_starts;

    for (uint32_t target : result.call_targets) {
        block_starts.insert(target);
    }
    for (uint32_t target : result.label_addresses) {
        block_starts.insert(target);
    }

    // Create blocks
    for (uint32_t start : block_starts) {
        if (!visited.count(start)) continue;

        BasicBlock block;
        block.start_address = get_offset(start);
        block.bank = get_bank(start);
        block.is_reachable = true;

        if (result.call_targets.count(start)) {
            block.is_function_entry = true;
        }

        // Check if this is an interrupt handler entry
        uint16_t offset = get_offset(start);
        if (offset == NES_VECTOR_NMI || offset == NES_VECTOR_RESET ||
            offset == NES_VECTOR_IRQ) {
            block.is_interrupt_entry = true;
        }

        // Find instructions in this block
        uint32_t curr = start;
        while (visited.count(curr)) {
            auto it = result.addr_to_index.find(curr);
            if (it == result.addr_to_index.end()) break;

            block.instruction_indices.push_back(it->second);
            const Instruction6502& instr = result.instructions[it->second];

            block.end_address = get_offset(curr) + instr.length;

            // Track successors for control flow
            if (instr.is_jump) {
                if (instr.mode == AddressMode::ABS ||
                    instr.mode == AddressMode::ABS_X ||
                    instr.mode == AddressMode::ABS_Y) {
                    block.successors.push_back(instr.imm16);
                } else if (instr.mode == AddressMode::REL) {
                    uint16_t target = get_offset(curr) + instr.length + instr.offset;
                    block.successors.push_back(target);
                }
                // Conditional jumps also fall through
                if (instr.is_conditional) {
                    block.successors.push_back(get_offset(curr) + instr.length);
                }
            } else if (instr.is_return && instr.is_conditional) {
                // Conditional returns fall through if condition is false
                block.successors.push_back(get_offset(curr) + instr.length);
            }

            // Check if this ends the block
            if (instr.is_jump || instr.is_return || instr.is_call ||
                instr.opcode_type == Opcode::BRK) {
                // CALLs fall through to next instruction after return
                if (instr.is_call) {
                    block.successors.push_back(get_offset(curr) + instr.length);
                }
                break;
            }

            curr = make_address(block.bank, get_offset(curr) + instr.length);

            // Check if next instruction starts a new block
            if (block_starts.count(curr)) {
                // Fall through to the new block - add as successor
                block.successors.push_back(get_offset(curr));
                break;
            }
        }

        result.blocks[start] = block;
    }

    // ============================================================================
    // Create Functions
    // ============================================================================

    std::set<uint32_t> processed_targets;
    const int MIN_FUNCTION_SIZE = 3;

    for (uint32_t target : result.call_targets) {
        if (processed_targets.count(target)) continue;

        auto block_it = result.blocks.find(target);
        if (block_it == result.blocks.end()) continue;

        Function func;
        func.name = generate_function_name(get_bank(target), get_offset(target));
        func.entry_address = get_offset(target);
        func.bank = get_bank(target);
        func.block_addresses.push_back(get_offset(target));

        // Check if this is an interrupt handler
        uint16_t entry = get_offset(target);
        if (entry == NES_VECTOR_NMI || entry == NES_VECTOR_IRQ) {
            func.is_interrupt_handler = true;
        }

        // Add all blocks reachable from this function (simple DFS)
        std::queue<uint32_t> func_queue;
        std::set<uint32_t> func_visited;
        func_queue.push(target);

        while (!func_queue.empty()) {
            uint32_t block_addr = func_queue.front();
            func_queue.pop();

            if (func_visited.count(block_addr)) continue;
            func_visited.insert(block_addr);

            auto blk = result.blocks.find(block_addr);
            if (blk == result.blocks.end()) continue;

            // Add this block to function if not already there
            if (block_addr != target) {
                func.block_addresses.push_back(get_offset(block_addr));
            }

            // Mark all reachable call targets as processed
            if (result.call_targets.count(block_addr) && block_addr != target) {
                processed_targets.insert(block_addr);
            }

            // Follow successors
            for (uint16_t succ : blk->second.successors) {
                uint32_t succ_addr = make_address(blk->second.bank, succ);
                if (!func_visited.count(succ_addr)) {
                    func_queue.push(succ_addr);
                }
            }
        }

        result.functions[target] = func;
        processed_targets.insert(target);
    }

    // Merge very small functions into their callers
    std::map<uint32_t, Function> merged_functions = result.functions;
    std::set<uint32_t> functions_to_remove;

    for (const auto& [func_addr, func] : result.functions) {
        if (functions_to_remove.count(func_addr)) continue;

        // Calculate total number of instructions in function
        int total_instrs = 0;
        for (uint16_t block_addr : func.block_addresses) {
            uint32_t full_blk_addr = make_address(func.bank, block_addr);
            auto blk_it = result.blocks.find(full_blk_addr);
            if (blk_it != result.blocks.end()) {
                total_instrs += blk_it->second.instruction_indices.size();
            }
        }

        // Check if this is a special entry point (vectors)
        bool is_special_entry = (func.bank == 0 && (
            func.entry_address == NES_VECTOR_RESET ||
            func.entry_address == NES_VECTOR_NMI ||
            func.entry_address == NES_VECTOR_IRQ
        ));

        if (total_instrs < MIN_FUNCTION_SIZE && !is_special_entry) {
            functions_to_remove.insert(func_addr);
        }
    }

    // Remove small functions
    for (uint32_t func_addr : functions_to_remove) {
        merged_functions.erase(func_addr);
        result.call_targets.erase(func_addr);
    }

    result.functions = merged_functions;

    // Update stats
    result.stats.total_instructions = result.instructions.size();
    result.stats.total_blocks = result.blocks.size();
    result.stats.total_functions = result.functions.size();

    return result;
}

AnalysisResult analyze_bank(const ROM& rom, uint8_t bank, const AnalyzerOptions& options) {
    (void)bank; // Unused parameter
    // For now, just analyze the whole ROM
    // TODO: Filter to specific bank
    return analyze(rom, options);
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

std::string generate_function_name(uint8_t bank, uint16_t address) {
    std::ostringstream ss;

    // Check for known NES entry points
    if (bank == 0) {
        switch (address) {
            case NES_VECTOR_RESET: return "reset_handler";
            case NES_VECTOR_NMI:   return "nmi_handler";
            case NES_VECTOR_IRQ:   return "irq_handler";
        }
    }

    ss << "func_";
    if (bank > 0) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)bank << "_";
    }
    ss << std::hex << std::setfill('0') << std::setw(4) << address;
    return ss.str();
}

std::string generate_label_name(uint8_t bank, uint16_t address) {
    std::ostringstream ss;
    ss << "loc_";
    if (bank > 0) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)bank << "_";
    }
    ss << std::hex << std::setfill('0') << std::setw(4) << address;
    return ss.str();
}

void print_analysis_summary(const AnalysisResult& result) {
    std::cout << "=== Analysis Summary ===" << std::endl;
    std::cout << "Total instructions: " << result.stats.total_instructions << std::endl;
    std::cout << "Total basic blocks: " << result.stats.total_blocks << std::endl;
    std::cout << "Total functions: " << result.stats.total_functions << std::endl;
    std::cout << "Call targets: " << result.call_targets.size() << std::endl;
    std::cout << "Label addresses: " << result.label_addresses.size() << std::endl;
    std::cout << "Bank switches detected: " << result.bank_tracker.switches().size() << std::endl;
    std::cout << "Cross-bank calls tracked: " << result.bank_tracker.calls().size() << std::endl;

    std::cout << "\nFunctions found:" << std::endl;
    for (const auto& [addr, func] : result.functions) {
        std::cout << "  " << func.name << " @ ";
        if (func.bank > 0) {
            std::cout << std::hex << std::setfill('0') << std::setw(2)
                      << (int)func.bank << ":";
        }
        std::cout << std::hex << std::setfill('0') << std::setw(4)
                  << func.entry_address << std::endl;
    }
}

bool is_likely_data(const AnalysisResult& result, uint8_t bank, uint16_t address) {
    uint32_t full_addr = AnalysisResult::make_addr(bank, address);
    return result.addr_to_index.find(full_addr) == result.addr_to_index.end();
}

} // namespace nesrecomp
