/**
 * @file main.cpp
 * @brief NES Recompiler CLI entry point
 */

#include "recompiler/rom.h"
#include "recompiler/decoder.h"
#include "recompiler/analyzer.h"
#include "recompiler/ir/ir.h"
#include "recompiler/ir/ir_builder.h"
#include "recompiler/codegen/c_emitter.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

void print_banner() {
    std::cout << R"(
   ____                      ____              ____                                _ _          _
  / ___| __ _ _ __ ___   ___| __ )  ___  _   _|  _ \ ___  ___ ___  _ __ ___  _ __ (_) | ___  __| |
 | |  _ / _` | '_ ` _ \ / _ \  _ \ / _ \| | | | |_) / _ \/ __/ _ \| '_ ` _ \| '_ \| | |/ _ \/ _` |
 | |_| | (_| | | | | | |  __/ |_) | (_) | |_| |  _ <  __/ (_| (_) | | | | | | |_) | | |  __/ (_| |
  \____|\__,_|_| |_| |_|\___|____/ \___/ \__, |_| \_\___|\___\___/|_| |_| |_| .__/|_|_|\___|\__,_|
                                         |___/                              |_|
)" << '\n';
    std::cout << "  NES Recompiler v0.1.0\n";
    std::cout << "  https://github.com/arcanite24/nes-recompiled\n\n";
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " <rom.nes> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <dir>    Output directory (default: <rom>_output)\n";
    std::cout << "  -d, --disasm          Disassemble only (don't generate code)\n";
    std::cout << "  -a, --analyze         Analyze control flow only\n";
    std::cout << "  -v, --verbose         Verbose output\n";
    std::cout << "  --trace               Trace execution analysis (very verbose)\n";
    std::cout << "  --limit <n>           Limit analysis to n instructions\n";
    std::cout << "  --single-function     Generate all code in a single function\n";
    std::cout << "  --no-comments         Don't include disassembly comments\n";
    std::cout << "  --add-entry-point <a> Add manual entry point (e.g. 0xC000)\n";
    std::cout << "  --no-scan             Disable aggressive code scanning (enabled by default)\n";
    std::cout << "  --use-trace <file>    Use runtime trace to find entry points\n";
    std::cout << "  -h, --help            Show this help\n";
}

std::string sanitize_prefix(const std::string& name) {
    std::string result = name;
    for (char& c : result) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    if (!result.empty() && isdigit(result[0])) {
        result = "_" + result;
    }
    return result;
}

int main(int argc, char* argv[]) {
    // Parse command line
    if (argc < 2) {
        print_banner();
        print_usage(argv[0]);
        return 1;
    }

    std::string rom_path;
    std::string output_dir;
    bool disasm_only = false;
    bool analyze_only = false;
    bool verbose = false;
    bool trace_log = false;
    size_t limit_instructions = 0;
    bool single_function = false;
    bool emit_comments = true;
    bool aggressive_scan = true;
    std::vector<uint32_t> manual_entry_points;
    std::string trace_file_path;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_dir = argv[++i];
            }
        } else if (arg == "-d" || arg == "--disasm") {
            disasm_only = true;
        } else if (arg == "-a" || arg == "--analyze") {
            analyze_only = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--trace") {
            trace_log = true;
        } else if (arg == "--limit") {
            if (i + 1 < argc) {
                limit_instructions = std::stoul(argv[++i]);
            }
        } else if (arg == "--single-function") {
            single_function = true;
        } else if (arg == "--no-comments") {
            emit_comments = false;
        } else if (arg == "--add-entry-point") {
            if (i + 1 < argc) {
                std::string param = argv[++i];
                try {
                    uint32_t addr = std::stoul(param, nullptr, 0);
                    manual_entry_points.push_back(addr);
                    std::cout << "Added manual entry point: 0x" << std::hex << addr << std::dec << "\n";
                } catch (...) {
                    std::cerr << "Invalid entry point format: " << param << "\n";
                }
            }
        } else if (arg == "--no-scan") {
            aggressive_scan = false;
        } else if (arg == "--use-trace") {
            if (i + 1 < argc) {
                trace_file_path = argv[++i];
            }
        } else if (arg[0] != '-') {
            rom_path = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (rom_path.empty()) {
        std::cerr << "Error: No ROM file specified\n";
        print_usage(argv[0]);
        return 1;
    }

    print_banner();

    // Load ROM
    std::cout << "Loading ROM: " << rom_path << "\n";

    auto rom_opt = gbrecomp::ROM::load(rom_path);
    if (!rom_opt) {
        std::cerr << "Error: Failed to load ROM\n";
        return 1;
    }

    auto& rom = *rom_opt;

    if (!rom.is_valid()) {
        std::cerr << "Error: " << rom.error() << "\n";
        return 1;
    }

    // Print ROM info
    gbrecomp::print_rom_info(rom);

    // Set default output directory
    if (output_dir.empty()) {
        output_dir = fs::path(rom_path).stem().string() + "_output";
    }

    // Disassemble only mode
    if (disasm_only) {
        std::cout << "\nDisassembly:\n";
        std::cout << "============\n";

        uint8_t bank = 0;
        auto instructions = gbrecomp::decode_bank(rom, bank);

        for (const auto& instr : instructions) {
            std::cout << instr.disassemble() << "\n";
        }
        return 0;
    }

    // Analyze control flow
    std::cout << "\nAnalyzing control flow...\n";

    gbrecomp::AnalyzerOptions analyze_opts;
    analyze_opts.trace_log = trace_log;
    analyze_opts.verbose = verbose;
    analyze_opts.max_instructions = limit_instructions;
    analyze_opts.entry_points = manual_entry_points;
    analyze_opts.aggressive_scan = aggressive_scan;
    analyze_opts.trace_file_path = trace_file_path;

    // Detect standard NES OAM DMA routine
    // Routine: LDA #6; STA $2003; LDA #0; STA $4014; RTS
    // Or the common pattern: LDA #$02; STA $2000; LDA #$00; STA $2006; ...
    const std::vector<uint8_t>& data = rom.bytes();

    // Common OAM DMA pattern: A9 02 8D 03 20 A9 00 8D 04 20 (LDA #2; STA $2003; LDA #0; STA $2004)
    // Another pattern: A9 XX 8D 03 20 A9 00 8D 04 20 (LDA #XX; STA $2003; LDA #0; STA $2004)
    // Look for STA $2003 (8D 03 20) followed by STA $2004 (8D 04 20)
    for (size_t i = 0; i + 10 < data.size(); i++) {
        if (data[i] == 0x8D && data[i+1] == 0x03 && data[i+2] == 0x20 &&
            data[i+6] == 0x8D && data[i+7] == 0x04 && data[i+8] == 0x20) {
            std::cout << "Detected OAM DMA setup at ROM offset 0x" << std::hex << i << std::dec << "\n";
            break;
        }
    }

    auto analysis = gbrecomp::analyze(rom, analyze_opts);

    std::cout << "  Found " << analysis.stats.total_functions << " functions\n";
    std::cout << "  Found " << analysis.stats.total_blocks << " basic blocks\n";
    std::cout << "  " << analysis.label_addresses.size() << " labels needed\n";

    if (verbose) {
        gbrecomp::print_analysis_summary(analysis);
    }

    if (analyze_only) {
        return 0;
    }

    // Build IR
    std::cout << "\nBuilding IR...\n";

    gbrecomp::ir::BuilderOptions ir_opts;
    ir_opts.emit_comments = emit_comments;

    gbrecomp::ir::IRBuilder builder(ir_opts);
    auto ir_program = builder.build(analysis, rom.name());

    std::cout << "  " << ir_program.blocks.size() << " IR blocks\n";
    std::cout << "  " << ir_program.functions.size() << " IR functions\n";

    // Generate code
    std::cout << "\nGenerating C code...\n";

    gbrecomp::codegen::GeneratorOptions gen_opts;
    gen_opts.output_prefix = sanitize_prefix(fs::path(rom_path).stem().string());
    gen_opts.output_dir = output_dir;
    gen_opts.emit_comments = emit_comments;
    gen_opts.single_function_mode = single_function;

    auto output = gbrecomp::codegen::generate_output(
        ir_program, rom.data(), rom.size(), gen_opts);

    // Create output directory
    fs::path out_path = output_dir;
    if (!fs::exists(out_path)) {
        std::cout << "Creating output directory: " << out_path << "\n";
        fs::create_directories(out_path);
    }

    // Write output files
    if (!gbrecomp::codegen::write_output(output, output_dir)) {
        std::cerr << "Error: Failed to write output files\n";
        return 1;
    }

    std::cout << "\nGenerated files:\n";
    std::cout << "  " << (out_path / output.header_file) << "\n";
    std::cout << "  " << (out_path / output.source_file) << "\n";
    std::cout << "  " << (out_path / output.main_file) << "\n";
    std::cout << "  " << (out_path / output.cmake_file) << "\n";
    if (!output.rom_data_file.empty()) {
        std::cout << "  " << (out_path / output.rom_data_file) << "\n";
    }

    std::cout << "\nBuild instructions:\n";
    std::cout << "  cd " << out_path << " && mkdir -p build && cd build && cmake -G Ninja .. && cmake --build . && ./" << gen_opts.output_prefix << "\n";

    return 0;
}
