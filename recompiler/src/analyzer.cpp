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
    // For NES, ROM is at $8000-$FFFF, not $0000-$7FFF like GameBoy
    if (addr < 0x8000) return false;  // This is RAM/IO region for NES

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

/**
 * @brief Pre-scan all banks for JSR/JMP targets to discover code
 */
static void prescan_for_targets(const ROM& rom, const AnalyzerOptions& options,
                                std::set<uint32_t>& call_targets,
                                std::set<uint32_t>& label_addresses) {
    uint16_t num_banks = rom.header().rom_banks;
    if (num_banks == 0) num_banks = 1;
    
    uint8_t mapper = rom.header().mapper_number;
    
    Decoder6502 decoder(rom);
    
    for (uint16_t bank_idx = 0; bank_idx < num_banks && bank_idx < 256; bank_idx++) {
        uint8_t bank = static_cast<uint8_t>(bank_idx);
        
        // Scan entire ROM address space for this bank ($8000-$FFFF)
        for (uint16_t addr = 0x8000; addr < 0xFFFA; addr++) {
            // Calculate ROM offset for this bank/address
            size_t rom_offset = 16 + static_cast<size_t>(bank) * 0x4000 + (addr - 0x8000);
            if (rom_offset >= rom.size()) break;
            
            uint8_t opcode = rom.read_banked(bank, addr);
            
            // JSR - absolute call
            if (opcode == 0x20 && addr + 2 < 0xFFFF) {
                uint16_t target = rom.read_banked(bank, addr + 1) |
                                 (rom.read_banked(bank, addr + 2) << 8);
                if (target >= 0x8000 && target < 0xFFFA) {
                    // For MMC1, we can't know which bank is active at runtime
                    // Add target to all banks to ensure we don't miss code
                    if (mapper == 1) {
                        for (uint16_t b = 0; b < num_banks && b < 256; b++) {
                            call_targets.insert(make_address(static_cast<uint8_t>(b), target));
                        }
                    } else {
                        // For NROM and other mappers, add in current bank context
                        call_targets.insert(make_address(bank, target));
                    }
                }
            }
            // JMP absolute
            else if (opcode == 0x4C && addr + 2 < 0xFFFF) {
                uint16_t target = rom.read_banked(bank, addr + 1) |
                                 (rom.read_banked(bank, addr + 2) << 8);
                if (target >= 0x8000 && target < 0xFFFA) {
                    // Same logic as JSR
                    if (mapper == 1) {
                        for (uint16_t b = 0; b < num_banks && b < 256; b++) {
                            label_addresses.insert(make_address(static_cast<uint8_t>(b), target));
                        }
                    } else {
                        label_addresses.insert(make_address(bank, target));
                    }
                }
            }
            // Branch instructions (relative) - stay within current bank
            else if ((opcode >= 0x10 && opcode <= 0x3F) &&
                     (opcode & 0x03) == 0x00 && opcode != 0x00) {
                // Branch opcodes: 0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0
                // BPL, BMI, BVC, BVS, BCC, BCS, BNE, BEQ
                if (addr + 1 < 0xFFFF) {
                    int8_t offset = static_cast<int8_t>(rom.read_banked(bank, addr + 1));
                    uint16_t target = addr + 2 + offset;
                    if (target >= 0x8000 && target < 0xFFFA) {
                        label_addresses.insert(make_address(bank, target));
                    }
                }
            }
        }
    }
    
    if (options.verbose) {
        std::cout << "[ANALYSIS] Pre-scan found " << call_targets.size()
                  << " JSR targets and " << label_addresses.size()
                  << " JMP/branch targets" << std::endl;
    }
}

/* ============================================================================
 * Copy Pattern Detection
 * ========================================================================== */

std::vector<CopyPattern> detect_copy_patterns(const AnalysisResult& result) {
    std::vector<CopyPattern> patterns;
    
    // Look for copy loop patterns in the instructions
    // Pattern: LDA from ROM, STA to RAM, with index register
    
    for (size_t i = 0; i + 5 < result.instructions.size(); i++) {
        const auto& instr = result.instructions[i];
        
        // Look for STA to RAM ($0000-$07FF)
        if (instr.opcode_type != Opcode::STA) continue;
        
        uint16_t dst_addr = 0;
        bool is_ram_dest = false;
        
        // Check addressing mode for RAM destination
        if (instr.mode == AddressMode::ZPG || instr.mode == AddressMode::ZPG_X ||
            instr.mode == AddressMode::ZPG_Y) {
            dst_addr = instr.imm8;
            is_ram_dest = true;  // Zero page is in RAM
        } else if (instr.mode == AddressMode::ABS || instr.mode == AddressMode::ABS_X ||
                   instr.mode == AddressMode::ABS_Y) {
            dst_addr = instr.imm16;
            is_ram_dest = (dst_addr < 0x0800);  // RAM is $0000-$07FF
        }
        
        if (!is_ram_dest) continue;
        
        // Look backwards for corresponding LDA from ROM
        int16_t src_offset = -1;
        uint16_t src_addr = 0;
        uint8_t src_bank = 0;
        
        for (int j = i - 1; j >= 0 && j >= static_cast<int>(i) - 10; j--) {
            const auto& prev_instr = result.instructions[j];
            
            if (prev_instr.opcode_type == Opcode::LDA) {
                // Check if it's reading from ROM
                if (prev_instr.mode == AddressMode::ABS || 
                    prev_instr.mode == AddressMode::ABS_X ||
                    prev_instr.mode == AddressMode::ABS_Y) {
                    src_addr = prev_instr.imm16;
                    src_bank = prev_instr.bank;
                    src_offset = static_cast<int16_t>(j - i);
                    break;
                }
            }
        }
        
        if (src_offset == -1) continue;
        
        // Check if source is in ROM ($8000-$FFFF)
        if (src_addr < 0x8000) continue;
        
        // Look for loop structure (DEY/INY + BNE)
        uint16_t loop_start = 0;
        uint16_t loop_end = 0;
        bool has_loop = false;
        
        for (size_t j = i + 1; j < std::min(i + 10, result.instructions.size()); j++) {
            const auto& next_instr = result.instructions[j];
            
            if (next_instr.opcode_type == Opcode::BNE || 
                next_instr.opcode_type == Opcode::BPL) {
                has_loop = true;
                loop_start = i + src_offset;  // Approximate
                loop_end = next_instr.address;
                break;
            }
        }
        
        // Look for jump to destination after copy (within next 50 instructions)
        uint16_t jump_target = 0;
        for (size_t j = i + 1; j < std::min(i + 50, result.instructions.size()); j++) {
            const auto& next_instr = result.instructions[j];
            
            if (next_instr.opcode_type == Opcode::JMP || 
                next_instr.opcode_type == Opcode::JSR) {
                // Check if jumping to RAM region near destination
                if (next_instr.imm16 >= 0x0000 && next_instr.imm16 < 0x0800) {
                    jump_target = next_instr.imm16;
                    break;
                }
            }
        }
        
        // Create pattern
        CopyPattern pattern;
        pattern.src_start = src_addr;
        pattern.dst_start = dst_addr;
        pattern.length = 1;  // Would need more analysis to determine actual length
        pattern.src_bank = src_bank;
        pattern.loop_start = loop_start;
        pattern.loop_end = loop_end;
        pattern.jump_target = jump_target;
        pattern.verified = (jump_target != 0);
        
        patterns.push_back(pattern);
    }
    
    return patterns;
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
    // For NES, we need to READ the vector values to get the actual entry points
    // Vectors are at $FFFA (NMI), $FFFC (Reset), $FFFE (IRQ)
    // But for ROMs, these are in the last bank

    // Calculate where vectors are in ROM file
    // For iNES: vectors are at the end of PRG ROM data
    // NES interrupt vectors occupy the last 6 bytes of PRG ROM:
    //   NMI:   bytes -6, -5 (address $FFFA-$FFFB)
    //   Reset: bytes -4, -3 (address $FFFC-$FFFD)
    //   IRQ:   bytes -2, -1 (address $FFFE-$FFFF)
    size_t prg_size = rom.header().prg_rom_bytes;
    uint8_t last_bank = 0;
    uint8_t mapper = rom.header().mapper_number;
    uint16_t rom_banks = rom.header().rom_banks;
    
    if (prg_size >= 6) {
        // For multi-bank ROMs, vectors are in the last bank
        // For NROM (mapper 0), vectors are always in bank 0
        if (mapper == 0) {
            // NROM: all code and vectors in bank 0
            last_bank = 0;
        } else {
            // MMC1 and other mappers: vectors in last bank
            last_bank = rom_banks - 1;
            if (last_bank == 0) last_bank = 1;
        }
        
        // Read NMI vector first (bytes -6, -5)
        uint8_t nmi_lo = rom.data()[16 + prg_size - 6];
        uint8_t nmi_hi = rom.data()[16 + prg_size - 5];
        uint16_t nmi_addr = (nmi_hi << 8) | nmi_lo;

        if (nmi_addr >= 0x8000 && options.follow_nmi) {
            // Add in last bank context for mapper ROMs
            result.call_targets.insert(make_address(last_bank, nmi_addr));
            if (options.verbose) {
                std::cout << "[ANALYSIS] NMI vector points to 0x" << std::hex << nmi_addr 
                          << " (bank " << (int)last_bank << ")" << std::dec << "\n";
            }
        }

        // Read reset vector (bytes -4, -3)
        uint8_t reset_lo = rom.data()[16 + prg_size - 4];
        uint8_t reset_hi = rom.data()[16 + prg_size - 3];
        uint16_t reset_addr = (reset_hi << 8) | reset_lo;

        if (reset_addr >= 0x8000) {
            // Valid ROM address - add as entry point in last bank for mapper ROMs
            result.call_targets.insert(make_address(last_bank, reset_addr));
            if (options.verbose) {
                std::cout << "[ANALYSIS] Reset vector points to 0x" << std::hex << reset_addr 
                          << " (bank " << (int)last_bank << ")" << std::dec << "\n";
            }
        }

        // Read IRQ vector (bytes -2, -1) - typically same as NMI for most games
        uint8_t irq_lo = rom.data()[16 + prg_size - 2];
        uint8_t irq_hi = rom.data()[16 + prg_size - 1];
        uint16_t irq_addr = (irq_hi << 8) | irq_lo;

        if (irq_addr >= 0x8000 && options.follow_irq) {
            result.call_targets.insert(make_address(last_bank, irq_addr));
            if (options.verbose) {
                std::cout << "[ANALYSIS] IRQ vector points to 0x" << std::hex << irq_addr 
                          << " (bank " << (int)last_bank << ")" << std::dec << "\n";
            }
        }
    }

    // Add manual entry points
    for (uint32_t addr : options.entry_points) {
        result.call_targets.insert(addr);
    }

    // Pre-scan all banks for JSR/JMP targets before main analysis
    // This discovers code that isn't reachable from reset vector alone
    if (options.aggressive_scan) {
        prescan_for_targets(rom, options, result.call_targets, result.label_addresses);
    }

    // Initial work queue seeding - preserve bank information!
    for (uint32_t target : result.call_targets) {
        uint8_t bank = get_bank(target);
        work_queue.push({target, bank});
    }

    // For ROMs with mappers, ensure all banks are analyzed
    if (rom.header().mapper_number > 0 && options.analyze_all_banks) {
        uint16_t num_banks = rom.header().rom_banks;
        for (uint16_t b = 0; b < num_banks && b < 256; b++) {
            uint8_t bank = static_cast<uint8_t>(b);
            // Add bank start as potential entry point
            uint16_t entry_addr = 0x8000;
            if (is_likely_valid_code(rom, bank, entry_addr)) {
                uint32_t bank_start = make_address(bank, entry_addr);
                if (result.call_targets.find(bank_start) == result.call_targets.end()) {
                    work_queue.push({bank_start, bank});
                    result.call_targets.insert(bank_start);
                }
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

            if (options.verbose) {
                std::cout << "[DEBUG] Visiting address " << std::hex << std::setfill('0')
                          << std::setw(2) << (int)bank << ":" << std::setw(4) << offset
                          << " (visited count: " << visited.size() << ")" << std::dec << "\n";
            }

            // NES memory map:
            // $0000-$07FF: Work RAM (not ROM)
            // $0800-$1FFF: RAM mirrors
            // $2000-$2007: PPU registers
            // $2008-$3FFF: PPU mirrors
            // $4000-$4017: APU/I/O
            // $4018-$FFFF: PRG ROM (mapper dependent)
            //
            // For NES, PRG ROM is at $8000-$FFFF, NOT $0000-$7FFF like GameBoy
            // Skip non-ROM regions
            if (offset < 0x8000 && offset >= 0x0000) {
                // This is RAM, PPU, or APU region - not ROM
                // Only skip if it's clearly not ROM data
                if (offset < 0x6000) continue;  // RAM and mirrors
                // $6000-$7FFF may be PRG RAM or ROM depending on mapper
            }

            // IMPORTANT: Preserve bank information for all ROM addresses
            // For mapper ROMs, the same address in different banks is different code
            // Only normalize addresses in the fixed regions
            
            // For MMC1 (mapper 1):
            // - $8000-$BFFF: Switchable bank (varies)
            // - $C000-$FFFF: Fixed to last bank
            uint8_t mapper = rom.header().mapper_number;
            uint16_t prg_banks = rom.header().rom_banks;
            uint8_t last_bank = (prg_banks > 1) ? (prg_banks - 1) : 0;
            
            if (offset < 0x4000) {
                // Fixed region below ROM - use bank 0
                bank = 0;
                addr = make_address(0, offset);
                if (visited.count(addr)) continue;
            } else if (offset >= 0xC000 && mapper == 1) {
                // MMC1: $C000-$FFFF is fixed to last bank
                // All accesses to this region go to the last bank
                bank = last_bank;
                addr = make_address(last_bank, offset);
                if (visited.count(addr)) continue;
            } else if (offset >= 0x8000) {
                // PRG ROM region ($8000-$BFFF for MMC1, or $8000-$FFFF for NROM)
                // Preserve bank information for switchable region
                if (bank == 0 && current_bank > 0) {
                    // Use current_bank context if available
                    bank = current_bank;
                    addr = make_address(bank, offset);
                }
                if (visited.count(addr)) continue;
            } else {
                // Switchable region ($4000-$7FFF) - use current bank context
                if (bank == 0) {
                    bank = current_bank > 0 ? current_bank : 1;
                    addr = make_address(bank, offset);
                }
                if (visited.count(addr)) continue;
                current_bank = bank;
            }

            // Calculate ROM offset
            // For NES: ROM data starts after 16-byte header
            // The bank number has already been determined above
            size_t rom_offset;
            
            if (offset < 0x8000) {
                // Switchable region below ROM - shouldn't happen for valid ROM code
                rom_offset = static_cast<size_t>(bank) * 0x4000 + (offset - 0x4000);
            } else {
                // PRG ROM region ($8000-$FFFF)
                // For multi-bank ROMs, each bank has its own 16KB region
                // Bank N's $8000-$FFFF maps to file offset: 16 + N * 0x4000 + (offset - 0x8000)
                // Note: For MMC1, $C000-$FFFF is fixed to last bank, but we've already
                // adjusted the bank number above, so we can use a simple formula here
                
                uint8_t mapper = rom.header().mapper_number;
                uint16_t prg_banks = rom.header().rom_banks;
                
                if (mapper == 0 && prg_banks == 1) {
                    // 16KB NROM: mirror entire $8000-$FFFF range to 0x0000-0x3FFF
                    rom_offset = 16 + ((offset - 0x8000) & 0x3FFF);
                } else if (mapper == 0 && prg_banks == 2) {
                    // 32KB NROM: no mirroring
                    rom_offset = 16 + (offset - 0x8000);
                } else {
                    // Multi-bank ROM (mapper 1, etc.)
                    // Simple linear mapping: bank * 16KB + offset within bank
                    rom_offset = 16 + static_cast<size_t>(bank) * 0x4000 + (offset - 0x8000);
                }
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

            // Debug: log all instructions in verbose mode
            if (options.verbose && instr.opcode == 0x20) {
                std::cout << "[DEBUG] Decoded JSR at " << std::hex << std::setfill('0')
                          << std::setw(2) << (int)bank << ":" << std::setw(4) << offset
                          << " -> target " << std::setw(4) << instr.imm16
                          << ", is_call=" << instr.is_call << std::dec << "\n";
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
            // For mapper ROMs, preserve the current bank context
            // For NROM (single bank), use bank 0
            auto target_bank = [&](uint16_t target) -> uint8_t {
                if (target < 0x8000) return 0;  // Not ROM
                // For MMC1 (mapper 1), $C000-$FFFF is fixed to last bank
                if (rom.header().mapper_number == 1 && target >= 0xC000) {
                    return rom.header().rom_banks - 1;
                }
                // For multi-bank ROMs, assume target is in current bank
                // This is a simplification - proper mapper support would track banking state
                return current_bank > 0 ? current_bank : 0;
            };

            /* ========================================================================
             * Control Flow Handling
             * ====================================================================== */

            if (instr.is_call) {
                // JSR - Jump to Subroutine
                uint16_t target = instr.imm16;
                
                // For MMC1, add JSR targets in ALL banks for switchable region ($8000-$BFFF)
                // This ensures we don't miss code due to unknown bank state at runtime
                if (rom.header().mapper_number == 1 && target >= 0x8000 && target < 0xC000) {
                    // Switchable region - analyze in all banks
                    for (uint16_t b = 0; b < rom.header().rom_banks && b < 256; b++) {
                        uint8_t tbank = static_cast<uint8_t>(b);
                        uint32_t target_addr = make_address(tbank, target);
                        
                        // Validate target
                        if (tbank > 0 && tbank != bank) {
                            if (!is_likely_valid_code(rom, tbank, target)) {
                                continue;  // Skip invalid targets
                            }
                        }
                        
                        if (options.verbose) {
                            std::cout << "[DEBUG] Found JSR at " << std::hex << std::setfill('0')
                                      << std::setw(2) << (int)bank << ":" << std::setw(4) << offset
                                      << " -> target " << std::setw(2) << (int)tbank << ":" 
                                      << std::setw(4) << target << std::dec << "\n";
                        }
                        result.call_targets.insert(target_addr);
                        work_queue.push({target_addr, tbank});
                        
                        // Track cross-bank calls (for mappers)
                        if (tbank != bank) {
                            result.stats.cross_bank_calls++;
                            result.bank_tracker.record_cross_bank_call(
                                addr, target_addr, bank, tbank);
                        }
                    }
                } else {
                    // Fixed region or other mappers - use normal bank resolution
                    uint8_t tbank = target_bank(target);
                    instr.resolved_target_bank = tbank;

                    // Validate target
                    if (tbank > 0 && tbank != bank) {
                        if (!is_likely_valid_code(rom, tbank, target)) {
                            // Skip invalid targets
                        }
                    }

                    if (options.verbose) {
                        std::cout << "[DEBUG] Found JSR at " << std::hex << std::setfill('0')
                                  << std::setw(2) << (int)bank << ":" << std::setw(4) << offset
                                  << " -> target " << std::setw(4) << target << std::dec << "\n";
                    }
                    result.call_targets.insert(make_address(tbank, target));
                    work_queue.push({make_address(tbank, target), tbank});

                    // Track cross-bank calls (for mappers)
                    if (tbank != bank) {
                        result.stats.cross_bank_calls++;
                        result.bank_tracker.record_cross_bank_call(
                            addr, make_address(tbank, target), bank, tbank);
                    }
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
                    
                    // For MMC1, add JMP targets in ALL banks for switchable region ($8000-$BFFF)
                    if (rom.header().mapper_number == 1 && target >= 0x8000 && target < 0xC000) {
                        // Switchable region - analyze in all banks
                        for (uint16_t b = 0; b < rom.header().rom_banks && b < 256; b++) {
                            uint8_t tbank = static_cast<uint8_t>(b);
                            uint32_t target_addr = make_address(tbank, target);
                            
                            if (target >= 0x4000 && target <= 0x7FFF) {
                                if (tbank > 0 && tbank != bank) {
                                    if (!is_likely_valid_code(rom, tbank, target)) {
                                        continue;  // Skip invalid
                                    }
                                }
                            }
                            
                            if (options.verbose) {
                                std::cout << "[DEBUG] Found JMP at " << std::hex << std::setfill('0')
                                          << std::setw(2) << (int)bank << ":" << std::setw(4) << offset
                                          << " -> target " << std::setw(2) << (int)tbank << ":" 
                                          << std::setw(4) << target << std::dec << "\n";
                            }
                            result.call_targets.insert(target_addr);
                            result.label_addresses.insert(target_addr);
                            work_queue.push({target_addr, tbank});
                        }
                    } else {
                        // Fixed region or other mappers - use normal bank resolution
                        uint8_t tbank = target_bank(target);
                        instr.resolved_target_bank = tbank;

                        if (target >= 0x4000 && target <= 0x7FFF) {
                            if (tbank > 0 && tbank != bank) {
                                if (!is_likely_valid_code(rom, tbank, target)) {
                                    // Skip invalid
                                }
                            }
                            if (options.verbose) {
                                std::cout << "[DEBUG] Found JMP at " << std::hex << std::setfill('0')
                                          << std::setw(2) << (int)bank << ":" << std::setw(4) << offset
                                          << " -> target " << std::setw(4) << target << std::dec << "\n";
                            }
                            result.call_targets.insert(make_address(tbank, target));
                        }
                        result.label_addresses.insert(make_address(tbank, target));
                        work_queue.push({make_address(tbank, target), tbank});
                    }

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
        // This scans all banks for code patterns that weren't discovered through control flow
        if (options.aggressive_scan && !scanning_pass) {
            scanning_pass = true;

            if (options.verbose) {
                std::cout << "[ANALYSIS] Starting aggressive scan for missing code..." << std::endl;
            }

            size_t found_count = 0;
            uint16_t num_banks = rom.header().rom_banks;
            if (num_banks == 0) num_banks = 1;

            // Scan all ROM banks
            for (uint16_t bank_idx = 0; bank_idx < num_banks && bank_idx < 256; bank_idx++) {
                uint8_t bank = static_cast<uint8_t>(bank_idx);

                // Scan ROM region ($8000-$FFFA)
                for (uint32_t addr = 0x8000; addr < 0xFFFA; addr += 2) {
                    uint32_t full_addr = make_address(bank, addr);

                    if (visited.count(full_addr)) {
                        continue;
                    }

                    // Skip padding
                    uint8_t byte = rom.read_banked(bank, addr);
                    if (byte == 0xFF || byte == 0x00) {
                        continue;
                    }

                    // Check if this looks like valid code
                    if (is_likely_valid_code(rom, bank, addr)) {
                        if (options.verbose) {
                            std::cout << "[ANALYSIS] Detected potential function at "
                                      << std::hex << (int)bank << ":" << addr << std::dec << "\n";
                        }

                        result.call_targets.insert(full_addr);
                        work_queue.push({full_addr, bank});
                        found_count++;
                    }
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

    if (options.verbose) {
        std::cout << "[DEBUG] call_targets size: " << result.call_targets.size() << std::endl;
        std::cout << "[DEBUG] label_addresses size: " << result.label_addresses.size() << std::endl;
        std::cout << "[DEBUG] call_targets:" << std::endl;
        for (uint32_t t : result.call_targets) {
            std::cout << "  0x" << std::hex << std::setfill('0') << std::setw(4) << get_offset(t) << std::dec << std::endl;
        }
    }

    std::set<uint32_t> block_starts;

    for (uint32_t target : result.call_targets) {
        block_starts.insert(target);
    }
    for (uint32_t target : result.label_addresses) {
        block_starts.insert(target);
    }

    // Create blocks
    for (uint32_t start : block_starts) {
        if (options.verbose) {
            std::cout << "[DEBUG] Creating block for 0x" << std::hex << std::setfill('0')
                      << std::setw(4) << get_offset(start) << ", visited=" << visited.count(start) << std::dec << std::endl;
        }
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
        if (options.verbose) {
            std::cout << "[DEBUG] Creating function for target 0x" << std::hex << std::setfill('0')
                      << std::setw(4) << get_offset(target) << std::dec << std::endl;
        }
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

        // Don't remove functions that are explicit call targets (JSR targets)
        // Only remove small functions that were discovered through aggressive scanning
        bool is_explicit_target = result.call_targets.count(func_addr) > 0;

        if (total_instrs < MIN_FUNCTION_SIZE && !is_special_entry && !is_explicit_target) {
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

    // Detect ROM→RAM copy patterns
    result.copy_patterns = detect_copy_patterns(result);
    
    // Create RAM mappings from verified copy patterns
    for (const auto& pattern : result.copy_patterns) {
        if (pattern.verified && pattern.jump_target != 0) {
            RAMCodeMapping mapping;
            mapping.ram_addr = pattern.jump_target;
            mapping.rom_addr = pattern.src_start + (pattern.jump_target - pattern.dst_start);
            mapping.rom_bank = pattern.src_bank;
            mapping.length = pattern.length;
            mapping.verified = true;
            result.ram_mappings.add_mapping(mapping);
            
            if (options.verbose) {
                std::cout << "[RAM] Detected copy: $" 
                          << std::hex << std::setfill('0')
                          << std::setw(2) << (int)pattern.src_bank << ":"
                          << std::setw(4) << pattern.src_start
                          << " -> $" << std::setw(4) << pattern.jump_target
                          << std::dec << std::endl;
            }
        }
    }
    
    if (options.verbose && !result.copy_patterns.empty()) {
        std::cout << "[RAM] Found " << result.copy_patterns.size() 
                  << " copy patterns, " << result.ram_mappings.size()
                  << " verified RAM mappings" << std::endl;
    }

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
