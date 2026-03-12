/**
 * @file test_analyzer.cpp
 * @brief Simple test program for the 6502 analyzer
 */

#include "recompiler/rom.h"
#include "recompiler/analyzer.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rom.nes>\n";
        return 1;
    }

    std::string rom_path = argv[1];

    // Load ROM
    std::cout << "Loading ROM: " << rom_path << "\n";
    auto rom_opt = nesrecomp::ROM::load(rom_path);
    if (!rom_opt) {
        std::cerr << "Error: Failed to load ROM\n";
        return 1;
    }

    auto& rom = *rom_opt;
    if (!rom.is_valid()) {
        std::cerr << "Error: " << rom.error() << "\n";
        return 1;
    }

    nesrecomp::print_rom_info(rom);

    // Analyze control flow
    std::cout << "\nAnalyzing control flow...\n";
    std::cout << "  Entry points:\n";
    std::cout << "    Reset: $FFFC\n";
    std::cout << "    NMI:   $FFFA\n";
    std::cout << "    IRQ:   $FFFE\n";

    nesrecomp::AnalyzerOptions opts;
    opts.verbose = false;
    opts.trace_log = false;
    opts.follow_nmi = true;
    opts.follow_irq = true;
    opts.aggressive_scan = false;  // Disable aggressive scanning for cleaner output

    auto result = nesrecomp::analyze(rom, opts);

    std::cout << "\n=== Analysis Results ===\n";
    std::cout << "Total instructions: " << result.stats.total_instructions << "\n";
    std::cout << "Total basic blocks: " << result.stats.total_blocks << "\n";
    std::cout << "Total functions:    " << result.stats.total_functions << "\n";
    std::cout << "Call targets:       " << result.call_targets.size() << "\n";
    std::cout << "Label addresses:    " << result.label_addresses.size() << "\n";

    // Print interrupt handlers
    std::cout << "\n=== Interrupt Vectors ===\n";
    
    // Read reset vector
    uint8_t reset_lo = rom.read_banked(0, 0xFFFC);
    uint8_t reset_hi = rom.read_banked(0, 0xFFFD);
    uint16_t reset_addr = reset_lo | (reset_hi << 8);
    std::cout << "Reset vector ($FFFC): $" << std::hex << reset_addr << std::dec << "\n";

    // Read NMI vector
    uint8_t nmi_lo = rom.read_banked(0, 0xFFFA);
    uint8_t nmi_hi = rom.read_banked(0, 0xFFFB);
    uint16_t nmi_addr = nmi_lo | (nmi_hi << 8);
    std::cout << "NMI vector ($FFFA):   $" << std::hex << nmi_addr << std::dec << "\n";

    // Read IRQ vector
    uint8_t irq_lo = rom.read_banked(0, 0xFFFE);
    uint8_t irq_hi = rom.read_banked(0, 0xFFFF);
    uint16_t irq_addr = irq_lo | (irq_hi << 8);
    std::cout << "IRQ vector ($FFFE):   $" << std::hex << irq_addr << std::dec << "\n";

    // Print functions found
    std::cout << "\n=== Functions Found ===\n";
    for (const auto& [addr, func] : result.functions) {
        std::cout << "  " << func.name << " @ $";
        std::cout << std::hex << std::setfill('0') << std::setw(4) << func.entry_address << std::dec;
        if (func.is_interrupt_handler) {
            std::cout << " [INTERRUPT]";
        }
        std::cout << " (" << func.block_addresses.size() << " blocks)\n";
    }

    // Print first few basic blocks
    std::cout << "\n=== Sample Basic Blocks ===\n";
    int count = 0;
    for (const auto& [addr, block] : result.blocks) {
        if (count >= 5) break;
        
        std::cout << "  Block @ $";
        std::cout << std::hex << std::setfill('0') << std::setw(4) << block.start_address << std::dec;
        std::cout << " - $";
        std::cout << std::hex << std::setfill('0') << std::setw(4) << block.end_address << std::dec;
        std::cout << " (" << block.instruction_indices.size() << " instrs";
        if (block.is_function_entry) std::cout << ", entry";
        if (block.is_interrupt_entry) std::cout << ", interrupt";
        std::cout << ")\n";
        
        // Print successors
        if (!block.successors.empty()) {
            std::cout << "    Successors: ";
            for (size_t i = 0; i < block.successors.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << "$" << std::hex << std::setfill('0') << std::setw(4) 
                          << block.successors[i] << std::dec;
            }
            std::cout << "\n";
        }
        
        count++;
    }

    return 0;
}
