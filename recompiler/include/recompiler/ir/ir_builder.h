/**
 * @file ir_builder.h
 * @brief Build IR from decoded 6502 instructions
 */

#ifndef RECOMPILER_IR_BUILDER_H
#define RECOMPILER_IR_BUILDER_H

#include "ir.h"
#include "../decoder_6502.h"
#include "../analyzer.h"
#include <vector>

namespace nesrecomp {
namespace ir {

/**
 * @brief Options for IR building
 */
struct BuilderOptions {
    bool emit_source_locations = true;   // Include source address info
    bool emit_comments = true;           // Include disassembly comments
    bool preserve_flags_exactly = true;  // Emit exact flag computations
};

/**
 * @brief Builds IR from analyzed instructions
 */
class IRBuilder {
public:
    explicit IRBuilder(const BuilderOptions& options = {});

    /**
     * @brief Build IR program from analysis result
     *
     * @param analysis Analyzed ROM
     * @param rom_name Name for the program
     * @return IR program
     */
    Program build(const AnalysisResult& analysis, const std::string& rom_name);

    /**
     * @brief Lower a single instruction to IR
     *
     * @param instr Decoded 6502 instruction
     * @param block Block to append to
     */
    void lower_instruction(const Instruction6502& instr, BasicBlock& block);

private:
    BuilderOptions options_;

    // Lowering helpers for instruction categories
    void lower_load(const Instruction6502& instr, BasicBlock& block);
    void lower_store(const Instruction6502& instr, BasicBlock& block);
    void lower_transfer(const Instruction6502& instr, BasicBlock& block);
    void lower_stack(const Instruction6502& instr, BasicBlock& block);
    void lower_alu(const Instruction6502& instr, BasicBlock& block);
    void lower_shift_rotate(const Instruction6502& instr, BasicBlock& block);
    void lower_inc_dec(const Instruction6502& instr, BasicBlock& block);
    void lower_compare(const Instruction6502& instr, BasicBlock& block);
    void lower_jump(const Instruction6502& instr, BasicBlock& block);
    void lower_branch(const Instruction6502& instr, BasicBlock& block);
    void lower_call_return(const Instruction6502& instr, BasicBlock& block);
    void lower_flag_control(const Instruction6502& instr, BasicBlock& block);
    void lower_misc(const Instruction6502& instr, BasicBlock& block);
    void lower_unofficial(const Instruction6502& instr, BasicBlock& block);

    // Helper to add instruction with source location
    void emit(BasicBlock& block, IRInstruction instr, const Instruction6502& src);
};

} // namespace ir
} // namespace nesrecomp

#endif // RECOMPILER_IR_BUILDER_H
