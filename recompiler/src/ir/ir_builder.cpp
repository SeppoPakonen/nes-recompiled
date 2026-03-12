/**
 * @file ir_builder.cpp
 * @brief IR builder implementation for 6502 (NES)
 */

#include "recompiler/ir/ir_builder.h"
#include <sstream>
#include <iomanip>

namespace nesrecomp {
namespace ir {

/* ============================================================================
 * Operand Factory Methods (from ir.h)
 * ========================================================================== */

Operand Operand::reg8(uint8_t r) {
    Operand op;
    op.type = OperandType::REG8;
    op.value.reg8 = r;
    return op;
}

Operand Operand::imm8(uint8_t v) {
    Operand op;
    op.type = OperandType::IMM8;
    op.value.imm8 = v;
    return op;
}

Operand Operand::imm16(uint16_t v) {
    Operand op;
    op.type = OperandType::IMM16;
    op.value.imm16 = v;
    return op;
}

Operand Operand::offset(int8_t o) {
    Operand op;
    op.type = OperandType::OFFSET;
    op.value.offset = o;
    return op;
}

Operand Operand::condition(uint8_t c) {
    Operand op;
    op.type = OperandType::COND;
    op.value.condition = c;
    return op;
}

Operand Operand::bit_idx(uint8_t b) {
    Operand op;
    op.type = OperandType::BIT_IDX;
    op.value.bit_idx = b;
    return op;
}

Operand Operand::mem_imm16(uint16_t addr) {
    Operand op;
    op.type = OperandType::MEM_IMM16;
    op.value.imm16 = addr;
    return op;
}

Operand Operand::mem_zpg(uint8_t addr) {
    Operand op;
    op.type = OperandType::MEM_ZPG;
    op.value.imm8 = addr;
    return op;
}

Operand Operand::mem_zpg_x(uint8_t addr) {
    Operand op;
    op.type = OperandType::MEM_ZPG_X;
    op.value.imm8 = addr;
    return op;
}

Operand Operand::mem_zpg_y(uint8_t addr) {
    Operand op;
    op.type = OperandType::MEM_ZPG_Y;
    op.value.imm8 = addr;
    return op;
}

Operand Operand::mem_abs_x(uint16_t addr) {
    Operand op;
    op.type = OperandType::MEM_ABS_X;
    op.value.imm16 = addr;
    return op;
}

Operand Operand::mem_abs_y(uint16_t addr) {
    Operand op;
    op.type = OperandType::MEM_ABS_Y;
    op.value.imm16 = addr;
    return op;
}

Operand Operand::mem_ind(uint16_t addr) {
    Operand op;
    op.type = OperandType::MEM_IND;
    op.value.imm16 = addr;
    return op;
}

Operand Operand::mem_ind_x(uint8_t addr) {
    Operand op;
    op.type = OperandType::MEM_IND_X;
    op.value.imm8 = addr;
    return op;
}

Operand Operand::mem_ind_y(uint8_t addr) {
    Operand op;
    op.type = OperandType::MEM_IND_Y;
    op.value.imm8 = addr;
    return op;
}

Operand Operand::label(uint32_t id) {
    Operand op;
    op.type = OperandType::LABEL_REF;
    op.value.label_id = id;
    return op;
}

/* ============================================================================
 * FlagEffects Factory Methods
 * ========================================================================== */

FlagEffects FlagEffects::none() {
    return FlagEffects{};
}

FlagEffects FlagEffects::nzvc() {
    FlagEffects f{};
    f.affects_n = f.affects_v = f.affects_z = f.affects_c = true;
    return f;
}

FlagEffects FlagEffects::nz() {
    FlagEffects f{};
    f.affects_n = f.affects_z = true;
    return f;
}

FlagEffects FlagEffects::nzc() {
    FlagEffects f{};
    f.affects_n = f.affects_z = f.affects_c = true;
    return f;
}

FlagEffects FlagEffects::nzv() {
    FlagEffects f{};
    f.affects_n = f.affects_z = f.affects_v = true;
    return f;
}

FlagEffects FlagEffects::c_only() {
    FlagEffects f{};
    f.affects_c = true;
    return f;
}

FlagEffects FlagEffects::d_only() {
    FlagEffects f{};
    f.affects_d = true;
    return f;
}

FlagEffects FlagEffects::i_only() {
    FlagEffects f{};
    f.affects_i = true;
    return f;
}

FlagEffects FlagEffects::v_only() {
    FlagEffects f{};
    f.affects_v = true;
    return f;
}

/* ============================================================================
 * IRInstruction Factory Methods
 * ========================================================================== */

IRInstruction IRInstruction::make_nop(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::NOP;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    return instr;
}

IRInstruction IRInstruction::make_label(uint32_t label_id) {
    IRInstruction instr;
    instr.opcode = Opcode::LABEL;
    instr.dst = Operand::label(label_id);
    return instr;
}

IRInstruction IRInstruction::make_comment(const std::string& text) {
    IRInstruction instr;
    instr.opcode = Opcode::NOP;
    instr.comment = text;
    return instr;
}

// Load instructions
IRInstruction IRInstruction::make_load_a_imm(uint8_t imm, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_A_IMM;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm8(imm);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_a_addr(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_A_ADDR;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm16(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_a_x(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_A_X;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::mem_abs_x(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_a_y(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_A_Y;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::mem_abs_y(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_x_imm(uint8_t imm, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_X_IMM;
    instr.dst = Operand::reg8(1);  // X
    instr.src = Operand::imm8(imm);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_x_addr(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_X_ADDR;
    instr.dst = Operand::reg8(1);  // X
    instr.src = Operand::imm16(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_y_imm(uint8_t imm, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_Y_IMM;
    instr.dst = Operand::reg8(2);  // Y
    instr.src = Operand::imm8(imm);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_load_y_addr(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LOAD_Y_ADDR;
    instr.dst = Operand::reg8(2);  // Y
    instr.src = Operand::imm16(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    instr.flags = FlagEffects::nz();
    return instr;
}

// Store instructions
IRInstruction IRInstruction::make_store_a_addr(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::STORE_A_ADDR;
    instr.dst = Operand::imm16(addr);
    instr.src = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    return instr;
}

IRInstruction IRInstruction::make_store_a_x(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::STORE_A_X;
    instr.dst = Operand::mem_abs_x(addr);
    instr.src = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 5;
    return instr;
}

IRInstruction IRInstruction::make_store_a_y(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::STORE_A_Y;
    instr.dst = Operand::mem_abs_y(addr);
    instr.src = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 5;
    return instr;
}

IRInstruction IRInstruction::make_store_x_addr(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::STORE_X_ADDR;
    instr.dst = Operand::imm16(addr);
    instr.src = Operand::reg8(1);  // X
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    return instr;
}

IRInstruction IRInstruction::make_store_y_addr(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::STORE_Y_ADDR;
    instr.dst = Operand::imm16(addr);
    instr.src = Operand::reg8(2);  // Y
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 4;
    return instr;
}

// Transfer instructions
IRInstruction IRInstruction::make_transfer_a_x(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::TRANSFER_A_X;
    instr.dst = Operand::reg8(1);  // X
    instr.src = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_transfer_a_y(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::TRANSFER_A_Y;
    instr.dst = Operand::reg8(2);  // Y
    instr.src = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_transfer_x_a(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::TRANSFER_X_A;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::reg8(1);  // X
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_transfer_y_a(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::TRANSFER_Y_A;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::reg8(2);  // Y
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_transfer_x_sp(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::TRANSFER_X_SP;
    instr.dst = Operand::reg8(3);  // SP
    instr.src = Operand::reg8(1);  // X
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    // No flags affected
    return instr;
}

IRInstruction IRInstruction::make_transfer_sp_x(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::TRANSFER_SP_X;
    instr.dst = Operand::reg8(1);  // X
    instr.src = Operand::reg8(3);  // SP
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

// Stack instructions
IRInstruction IRInstruction::make_push_a(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::PUSH_A;
    instr.src = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 3;
    return instr;
}

IRInstruction IRInstruction::make_push_sr(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::PUSH_SR;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 3;
    return instr;
}

IRInstruction IRInstruction::make_pull_a(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::PULL_A;
    instr.dst = Operand::reg8(0);  // A
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 4;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_pull_sr(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::PULL_SR;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 4;
    // Affects all flags based on pulled value
    return instr;
}

// ALU instructions
IRInstruction IRInstruction::make_adc(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::ADC;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzvc();
    return instr;
}

IRInstruction IRInstruction::make_sbc(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::SBC;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzvc();
    return instr;
}

IRInstruction IRInstruction::make_and(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::AND;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_ora(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::ORA;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_eor(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::EOR;
    instr.dst = Operand::reg8(0);  // A
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_bit(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BIT;
    instr.dst = Operand::reg8(0);  // A (implicit)
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzv();
    return instr;
}

IRInstruction IRInstruction::make_cmp(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CMP;
    instr.dst = Operand::reg8(0);  // A (implicit)
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

IRInstruction IRInstruction::make_cpx(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CPX;
    instr.dst = Operand::reg8(1);  // X (implicit)
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

IRInstruction IRInstruction::make_cpy(uint8_t src, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CPY;
    instr.dst = Operand::reg8(2);  // Y (implicit)
    instr.src = Operand::imm8(src);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

// Shift/Rotate instructions
IRInstruction IRInstruction::make_asl(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::ASL;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

IRInstruction IRInstruction::make_lsr(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::LSR;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

IRInstruction IRInstruction::make_rol(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::ROL;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

IRInstruction IRInstruction::make_ror(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::ROR;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nzc();
    return instr;
}

// Increment/Decrement
IRInstruction IRInstruction::make_inc(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::INC;
    instr.dst = Operand::imm16(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 6;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_dec(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::DEC;
    instr.dst = Operand::imm16(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 6;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_inc_x(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::INC_X;
    instr.dst = Operand::reg8(1);  // X
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_dec_x(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::DEC_X;
    instr.dst = Operand::reg8(1);  // X
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_inc_y(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::INC_Y;
    instr.dst = Operand::reg8(2);  // Y
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

IRInstruction IRInstruction::make_dec_y(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::DEC_Y;
    instr.dst = Operand::reg8(2);  // Y
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::nz();
    return instr;
}

// Jump instructions
IRInstruction IRInstruction::make_jmp_abs(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::JMP_ABS;
    instr.dst = Operand::label(label_id);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 3;
    return instr;
}

IRInstruction IRInstruction::make_jmp_ind(uint16_t addr, uint8_t bank, uint16_t src_addr) {
    IRInstruction instr;
    instr.opcode = Opcode::JMP_IND;
    instr.dst = Operand::mem_ind(addr);
    instr.source_bank = bank;
    instr.source_address = src_addr;
    instr.cycles = 5;
    return instr;
}

IRInstruction IRInstruction::make_jsr(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::JSR;
    instr.dst = Operand::label(label_id);
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 6;
    return instr;
}

IRInstruction IRInstruction::make_rts(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::RTS;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 6;
    return instr;
}

IRInstruction IRInstruction::make_rti(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::RTI;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 6;
    return instr;
}

// Branch instructions
IRInstruction IRInstruction::make_bcc(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BCC;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(5);  // NC - Not Carry
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_bcs(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BCS;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(4);  // C - Carry
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_beq(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BEQ;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(2);  // Z - Zero
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_bne(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BNE;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(3);  // NZ - Not Zero
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_bmi(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BMI;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(0);  // N - Negative
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_bpl(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BPL;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(1);  // P - Positive
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_bvc(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BVC;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(7);  // NV - Not Overflow
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

IRInstruction IRInstruction::make_bvs(uint32_t label_id, uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BVS;
    instr.dst = Operand::label(label_id);
    instr.src = Operand::condition(6);  // V - Overflow
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.cycles_branch_taken = 3;
    return instr;
}

// Flag control instructions
IRInstruction IRInstruction::make_clc(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CLC;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::c_only();
    instr.flags.fixed_c = true;
    instr.flags.c_value = false;
    return instr;
}

IRInstruction IRInstruction::make_sec(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::SEC;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::c_only();
    instr.flags.fixed_c = true;
    instr.flags.c_value = true;
    return instr;
}

IRInstruction IRInstruction::make_cli(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CLI;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::i_only();
    instr.flags.fixed_i = true;
    instr.flags.i_value = false;
    return instr;
}

IRInstruction IRInstruction::make_sei(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::SEI;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::i_only();
    instr.flags.fixed_i = true;
    instr.flags.i_value = true;
    return instr;
}

IRInstruction IRInstruction::make_cld(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CLD;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::d_only();
    instr.flags.fixed_d = true;
    instr.flags.d_value = false;
    return instr;
}

IRInstruction IRInstruction::make_sed(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::SED;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::d_only();
    instr.flags.fixed_d = true;
    instr.flags.d_value = true;
    return instr;
}

IRInstruction IRInstruction::make_clv(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::CLV;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 2;
    instr.flags = FlagEffects::v_only();
    instr.flags.fixed_v = true;
    instr.flags.v_value = false;
    return instr;
}

// Special
IRInstruction IRInstruction::make_brk(uint8_t bank, uint16_t addr) {
    IRInstruction instr;
    instr.opcode = Opcode::BRK;
    instr.source_bank = bank;
    instr.source_address = addr;
    instr.cycles = 7;
    return instr;
}

/* ============================================================================
 * Program Methods
 * ========================================================================== */

uint32_t Program::create_block(uint8_t bank, uint16_t addr) {
    uint32_t id = next_block_id++;
    BasicBlock block;
    block.id = id;
    block.bank = bank;
    block.start_address = addr;
    block.end_address = addr;
    block.label = make_address_label(bank, addr);
    blocks[id] = block;
    return id;
}

uint32_t Program::create_label(const std::string& name) {
    uint32_t id = next_label_id++;
    labels[id] = name;
    label_by_name[name] = id;
    return id;
}

uint32_t Program::get_or_create_label(const std::string& name) {
    auto it = label_by_name.find(name);
    if (it != label_by_name.end()) {
        return it->second;
    }
    return create_label(name);
}

std::string Program::get_label_name(uint32_t id) const {
    auto it = labels.find(id);
    if (it != labels.end()) {
        return it->second;
    }
    return "unknown_label";
}

std::string Program::make_address_label(uint8_t bank, uint16_t addr) const {
    std::ostringstream ss;
    ss << "loc_";
    if (bank > 0) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)bank << "_";
    }
    ss << std::hex << std::setfill('0') << std::setw(4) << addr;
    return ss.str();
}

std::string Program::make_function_name(uint8_t bank, uint16_t addr) const {
    std::ostringstream ss;
    ss << "func_";
    if (bank > 0) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)bank << "_";
    }
    ss << std::hex << std::setfill('0') << std::setw(4) << addr;
    return ss.str();
}

/* ============================================================================
 * IRBuilder Implementation
 * ========================================================================== */

IRBuilder::IRBuilder(const BuilderOptions& options) : options_(options) {}

Program IRBuilder::build(const AnalysisResult& analysis, const std::string& rom_name) {
    Program program;
    program.rom_name = rom_name;
    program.main_entry = analysis.entry_point;
    program.interrupt_vectors = analysis.interrupt_vectors;

    // For each function in analysis, create IR function
    for (const auto& [addr, func] : analysis.functions) {
        ir::Function ir_func;
        ir_func.name = func.name;
        ir_func.bank = func.bank;
        ir_func.entry_address = func.entry_address;
        ir_func.is_interrupt_handler = func.is_interrupt_handler;

        // Create a block for each block address in this function
        for (uint16_t block_addr : func.block_addresses) {
            uint32_t full_addr = (static_cast<uint32_t>(func.bank) << 16) | block_addr;
            auto block_it = analysis.blocks.find(full_addr);
            if (block_it == analysis.blocks.end()) continue;

            const nesrecomp::BasicBlock& src_block = block_it->second;

            // Create new IR block for this address
            uint32_t block_id = program.create_block(func.bank, block_addr);
            ir_func.block_ids.push_back(block_id);
            ir::BasicBlock& dst_block = program.blocks[block_id];

            // Copy end_address from source block for correct fallthrough handling
            dst_block.end_address = src_block.end_address;

            // Lower each instruction in the block
            for (size_t idx : src_block.instruction_indices) {
                if (idx < analysis.instructions.size()) {
                    const Instruction6502& instr = analysis.instructions[idx];
                    lower_instruction(instr, dst_block);
                }
            }
        }

        program.functions[func.name] = ir_func;
    }

    return program;
}

void IRBuilder::lower_instruction(const Instruction6502& instr, ir::BasicBlock& block) {
    // Add a comment with the disassembly
    if (options_.emit_comments) {
        IRInstruction comment;
        comment.opcode = ir::Opcode::NOP;
        comment.comment = instr.disassemble();
        comment.source_bank = instr.bank;
        comment.source_address = instr.address;
        block.instructions.push_back(comment);
    }

    // Lower based on opcode type (using nesrecomp::Opcode from decoder_6502.h)
    switch (instr.opcode_type) {
        // Load instructions
        case nesrecomp::Opcode::LDA:
        case nesrecomp::Opcode::LDX:
        case nesrecomp::Opcode::LDY:
        case nesrecomp::Opcode::LAX:
            lower_load(instr, block);
            break;

        // Store instructions
        case nesrecomp::Opcode::STA:
        case nesrecomp::Opcode::STX:
        case nesrecomp::Opcode::STY:
        case nesrecomp::Opcode::SAX:
        case nesrecomp::Opcode::SHA:
        case nesrecomp::Opcode::SHS:
        case nesrecomp::Opcode::SHX:
        case nesrecomp::Opcode::SHY:
            lower_store(instr, block);
            break;

        // Transfer instructions
        case nesrecomp::Opcode::TAX:
        case nesrecomp::Opcode::TAY:
        case nesrecomp::Opcode::TXA:
        case nesrecomp::Opcode::TYA:
        case nesrecomp::Opcode::TSX:
        case nesrecomp::Opcode::TXS:
            lower_transfer(instr, block);
            break;

        // Stack instructions
        case nesrecomp::Opcode::PHA:
        case nesrecomp::Opcode::PHP:
        case nesrecomp::Opcode::PLA:
        case nesrecomp::Opcode::PLP:
            lower_stack(instr, block);
            break;

        // ALU instructions
        case nesrecomp::Opcode::ADC:
        case nesrecomp::Opcode::SBC:
        case nesrecomp::Opcode::AND:
        case nesrecomp::Opcode::ORA:
        case nesrecomp::Opcode::EOR:
        case nesrecomp::Opcode::BIT:
            lower_alu(instr, block);
            break;

        // Shift/Rotate instructions
        case nesrecomp::Opcode::ASL:
        case nesrecomp::Opcode::LSR:
        case nesrecomp::Opcode::ROL:
        case nesrecomp::Opcode::ROR:
        case nesrecomp::Opcode::ALR:
        case nesrecomp::Opcode::ARR:
            lower_shift_rotate(instr, block);
            break;

        // Increment/Decrement
        case nesrecomp::Opcode::INC:
        case nesrecomp::Opcode::DEC:
        case nesrecomp::Opcode::INX:
        case nesrecomp::Opcode::DEX:
        case nesrecomp::Opcode::INY:
        case nesrecomp::Opcode::DEY:
            lower_inc_dec(instr, block);
            break;

        // Compare instructions
        case nesrecomp::Opcode::CMP:
        case nesrecomp::Opcode::CPX:
        case nesrecomp::Opcode::CPY:
        case nesrecomp::Opcode::DCP:
        case nesrecomp::Opcode::AXS:
            lower_compare(instr, block);
            break;

        // Jump instructions
        case nesrecomp::Opcode::JMP:
            lower_jump(instr, block);
            break;

        // Branch instructions
        case nesrecomp::Opcode::BCC:
        case nesrecomp::Opcode::BCS:
        case nesrecomp::Opcode::BEQ:
        case nesrecomp::Opcode::BNE:
        case nesrecomp::Opcode::BMI:
        case nesrecomp::Opcode::BPL:
        case nesrecomp::Opcode::BVC:
        case nesrecomp::Opcode::BVS:
            lower_branch(instr, block);
            break;

        // Call/Return instructions
        case nesrecomp::Opcode::JSR:
        case nesrecomp::Opcode::RTS:
        case nesrecomp::Opcode::RTI:
        case nesrecomp::Opcode::BRK:
            lower_call_return(instr, block);
            break;

        // Flag control
        case nesrecomp::Opcode::CLC:
        case nesrecomp::Opcode::SEC:
        case nesrecomp::Opcode::CLI:
        case nesrecomp::Opcode::SEI:
        case nesrecomp::Opcode::CLD:
        case nesrecomp::Opcode::SED:
        case nesrecomp::Opcode::CLV:
            lower_flag_control(instr, block);
            break;

        // Misc
        case nesrecomp::Opcode::NOP:
            emit(block, IRInstruction::make_nop(instr.bank, instr.address), instr);
            break;

        // Unofficial opcodes
        case nesrecomp::Opcode::SLO:
        case nesrecomp::Opcode::RLA:
        case nesrecomp::Opcode::SRE:
        case nesrecomp::Opcode::RRA:
        case nesrecomp::Opcode::ISB:
        case nesrecomp::Opcode::LAS:
        case nesrecomp::Opcode::ANE:
        case nesrecomp::Opcode::ANC:
        case nesrecomp::Opcode::SKB:
        case nesrecomp::Opcode::IGN:
            lower_unofficial(instr, block);
            break;

        default:
            // Emit NOP for unhandled instructions
            emit(block, IRInstruction::make_nop(instr.bank, instr.address), instr);
            break;
    }
}

void IRBuilder::emit(ir::BasicBlock& block, IRInstruction ir_instr, const Instruction6502& src) {
    if (options_.emit_source_locations) {
        ir_instr.source_bank = src.bank;
        ir_instr.source_address = src.address;
        ir_instr.has_source_location = true;
    }
    block.instructions.push_back(ir_instr);
}

// ========== Lowering Functions ==========

void IRBuilder::lower_load(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint16_t addr = instr.address;
    uint8_t bank = instr.bank;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::LDA:
            switch (instr.mode) {
                case AddressMode::IMT:
                    ir = IRInstruction::make_load_a_imm(instr.imm8, bank, addr);
                    break;
                case AddressMode::ZPG:
                    ir = IRInstruction::make_load_a_addr(instr.imm8, bank, addr);
                    ir.src = Operand::mem_zpg(instr.imm8);
                    break;
                case AddressMode::ZPG_X:
                    ir = IRInstruction::make_load_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::LOAD_A_X;
                    ir.src = Operand::mem_zpg_x(instr.imm8);
                    break;
                case AddressMode::ZPG_Y:
                    ir = IRInstruction::make_load_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::LOAD_A_Y;
                    ir.src = Operand::mem_zpg_y(instr.imm8);
                    break;
                case AddressMode::ABS:
                    ir = IRInstruction::make_load_a_addr(instr.imm16, bank, addr);
                    break;
                case AddressMode::ABS_X:
                    ir = IRInstruction::make_load_a_x(instr.imm16, bank, addr);
                    break;
                case AddressMode::ABS_Y:
                    ir = IRInstruction::make_load_a_y(instr.imm16, bank, addr);
                    break;
                case AddressMode::IDX_IND:
                    ir = IRInstruction::make_load_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::LOAD_A_X;
                    ir.src = Operand::mem_ind_x(instr.imm8);
                    break;
                case AddressMode::IND_IDX:
                    ir = IRInstruction::make_load_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::LOAD_A_Y;
                    ir.src = Operand::mem_ind_y(instr.imm8);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::LDX:
            switch (instr.mode) {
                case AddressMode::IMT:
                    ir = IRInstruction::make_load_x_imm(instr.imm8, bank, addr);
                    break;
                case AddressMode::ZPG:
                case AddressMode::ZPG_Y:
                    ir = IRInstruction::make_load_x_addr(instr.imm8, bank, addr);
                    ir.src = Operand::mem_zpg(instr.imm8);
                    break;
                case AddressMode::ABS:
                case AddressMode::ABS_Y:
                    ir = IRInstruction::make_load_x_addr(instr.imm16, bank, addr);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::LDY:
            switch (instr.mode) {
                case AddressMode::IMT:
                    ir = IRInstruction::make_load_y_imm(instr.imm8, bank, addr);
                    break;
                case AddressMode::ZPG:
                case AddressMode::ZPG_X:
                    ir = IRInstruction::make_load_y_addr(instr.imm8, bank, addr);
                    ir.src = Operand::mem_zpg(instr.imm8);
                    break;
                case AddressMode::ABS:
                case AddressMode::ABS_X:
                    ir = IRInstruction::make_load_y_addr(instr.imm16, bank, addr);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::LAX:
            // Unofficial: Load both A and X
            switch (instr.mode) {
                case AddressMode::IMT:
                    ir = IRInstruction::make_load_a_imm(instr.imm8, bank, addr);
                    emit(block, ir, instr);
                    ir = IRInstruction::make_transfer_a_x(bank, addr);
                    break;
                case AddressMode::ZPG:
                    ir = IRInstruction::make_load_a_addr(instr.imm8, bank, addr);
                    ir.src = Operand::mem_zpg(instr.imm8);
                    emit(block, ir, instr);
                    ir = IRInstruction::make_transfer_a_x(bank, addr);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        default:
            emit(block, IRInstruction::make_nop(bank, addr), instr);
            break;
    }
}

void IRBuilder::lower_store(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint16_t addr = instr.address;
    uint8_t bank = instr.bank;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::STA:
            switch (instr.mode) {
                case AddressMode::ZPG:
                    ir = IRInstruction::make_store_a_addr(instr.imm8, bank, addr);
                    break;
                case AddressMode::ZPG_X:
                    ir = IRInstruction::make_store_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::STORE_A_X;
                    ir.dst = Operand::mem_zpg_x(instr.imm8);
                    break;
                case AddressMode::ZPG_Y:
                    ir = IRInstruction::make_store_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::STORE_A_Y;
                    ir.dst = Operand::mem_zpg_y(instr.imm8);
                    break;
                case AddressMode::ABS:
                    ir = IRInstruction::make_store_a_addr(instr.imm16, bank, addr);
                    break;
                case AddressMode::ABS_X:
                    ir = IRInstruction::make_store_a_x(instr.imm16, bank, addr);
                    break;
                case AddressMode::ABS_Y:
                    ir = IRInstruction::make_store_a_y(instr.imm16, bank, addr);
                    break;
                case AddressMode::IDX_IND:
                    ir = IRInstruction::make_store_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::STORE_A_X;
                    ir.dst = Operand::mem_ind_x(instr.imm8);
                    break;
                case AddressMode::IND_IDX:
                    ir = IRInstruction::make_store_a_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::STORE_A_Y;
                    ir.dst = Operand::mem_ind_y(instr.imm8);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::STX:
            switch (instr.mode) {
                case AddressMode::ZPG:
                    ir = IRInstruction::make_store_x_addr(instr.imm8, bank, addr);
                    break;
                case AddressMode::ZPG_Y:
                    ir = IRInstruction::make_store_x_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::STORE_X_ADDR;
                    ir.dst = Operand::mem_zpg_y(instr.imm8);
                    break;
                case AddressMode::ABS:
                    ir = IRInstruction::make_store_x_addr(instr.imm16, bank, addr);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::STY:
            switch (instr.mode) {
                case AddressMode::ZPG:
                    ir = IRInstruction::make_store_y_addr(instr.imm8, bank, addr);
                    break;
                case AddressMode::ZPG_X:
                    ir = IRInstruction::make_store_y_addr(instr.imm8, bank, addr);
                    ir.opcode = ir::Opcode::STORE_Y_ADDR;
                    ir.dst = Operand::mem_zpg_x(instr.imm8);
                    break;
                case AddressMode::ABS:
                    ir = IRInstruction::make_store_y_addr(instr.imm16, bank, addr);
                    break;
                default:
                    ir = IRInstruction::make_nop(bank, addr);
                    break;
            }
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        default:
            // Unofficial store opcodes - emit as NOP with comment
            emit(block, IRInstruction::make_nop(bank, addr), instr);
            break;
    }
}

void IRBuilder::lower_transfer(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::TAX:
            ir = IRInstruction::make_transfer_a_x(bank, addr);
            break;
        case nesrecomp::Opcode::TAY:
            ir = IRInstruction::make_transfer_a_y(bank, addr);
            break;
        case nesrecomp::Opcode::TXA:
            ir = IRInstruction::make_transfer_x_a(bank, addr);
            break;
        case nesrecomp::Opcode::TYA:
            ir = IRInstruction::make_transfer_y_a(bank, addr);
            break;
        case nesrecomp::Opcode::TSX:
            ir = IRInstruction::make_transfer_sp_x(bank, addr);
            break;
        case nesrecomp::Opcode::TXS:
            ir = IRInstruction::make_transfer_x_sp(bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_stack(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::PHA:
            ir = IRInstruction::make_push_a(bank, addr);
            break;
        case nesrecomp::Opcode::PHP:
            ir = IRInstruction::make_push_sr(bank, addr);
            break;
        case nesrecomp::Opcode::PLA:
            ir = IRInstruction::make_pull_a(bank, addr);
            break;
        case nesrecomp::Opcode::PLP:
            ir = IRInstruction::make_pull_sr(bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_alu(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::ADC:
            ir = IRInstruction::make_adc(instr.imm8, bank, addr);
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::SBC:
            ir = IRInstruction::make_sbc(instr.imm8, bank, addr);
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::AND:
            ir = IRInstruction::make_and(instr.imm8, bank, addr);
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::ORA:
            ir = IRInstruction::make_ora(instr.imm8, bank, addr);
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::EOR:
            ir = IRInstruction::make_eor(instr.imm8, bank, addr);
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        case nesrecomp::Opcode::BIT:
            ir = IRInstruction::make_bit(instr.imm8, bank, addr);
            ir.cycles = instr.cycles;
            emit(block, ir, instr);
            break;

        default:
            emit(block, IRInstruction::make_nop(bank, addr), instr);
            break;
    }
}

void IRBuilder::lower_shift_rotate(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::ASL:
        case nesrecomp::Opcode::ALR:
            ir = IRInstruction::make_asl(bank, addr);
            break;
        case nesrecomp::Opcode::LSR:
            ir = IRInstruction::make_lsr(bank, addr);
            break;
        case nesrecomp::Opcode::ROL:
            ir = IRInstruction::make_rol(bank, addr);
            break;
        case nesrecomp::Opcode::ROR:
        case nesrecomp::Opcode::ARR:
            ir = IRInstruction::make_ror(bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_inc_dec(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::INC:
            ir = IRInstruction::make_inc(instr.imm16, bank, addr);
            break;
        case nesrecomp::Opcode::DEC:
            ir = IRInstruction::make_dec(instr.imm16, bank, addr);
            break;
        case nesrecomp::Opcode::INX:
            ir = IRInstruction::make_inc_x(bank, addr);
            break;
        case nesrecomp::Opcode::DEX:
            ir = IRInstruction::make_dec_x(bank, addr);
            break;
        case nesrecomp::Opcode::INY:
            ir = IRInstruction::make_inc_y(bank, addr);
            break;
        case nesrecomp::Opcode::DEY:
            ir = IRInstruction::make_dec_y(bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_compare(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::CMP:
            ir = IRInstruction::make_cmp(instr.imm8, bank, addr);
            break;
        case nesrecomp::Opcode::CPX:
            ir = IRInstruction::make_cpx(instr.imm8, bank, addr);
            break;
        case nesrecomp::Opcode::CPY:
            ir = IRInstruction::make_cpy(instr.imm8, bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_jump(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    if (instr.mode == AddressMode::IND) {
        ir = IRInstruction::make_jmp_ind(instr.imm16, bank, addr);
    } else {
        // Create label for jump target
        uint32_t label_id = 0;  // Will be resolved later
        ir = IRInstruction::make_jmp_abs(label_id, bank, addr);
        ir.dst = Operand::imm16(instr.imm16);
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_branch(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    // Calculate target address from relative offset
    uint16_t target = addr + instr.length + instr.offset;
    uint32_t label_id = 0;  // Will be resolved later

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::BCC:
            ir = IRInstruction::make_bcc(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BCS:
            ir = IRInstruction::make_bcs(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BEQ:
            ir = IRInstruction::make_beq(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BNE:
            ir = IRInstruction::make_bne(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BMI:
            ir = IRInstruction::make_bmi(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BPL:
            ir = IRInstruction::make_bpl(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BVC:
            ir = IRInstruction::make_bvc(label_id, bank, addr);
            break;
        case nesrecomp::Opcode::BVS:
            ir = IRInstruction::make_bvs(label_id, bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.dst = Operand::imm16(target);
    ir.cycles = instr.cycles;
    ir.cycles_branch_taken = instr.cycles_branch;
    emit(block, ir, instr);
}

void IRBuilder::lower_call_return(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::JSR:
            {
                uint32_t label_id = 0;  // Will be resolved later
                ir = IRInstruction::make_jsr(label_id, bank, addr);
                ir.dst = Operand::imm16(instr.imm16);
            }
            break;
        case nesrecomp::Opcode::RTS:
            ir = IRInstruction::make_rts(bank, addr);
            break;
        case nesrecomp::Opcode::RTI:
            ir = IRInstruction::make_rti(bank, addr);
            break;
        case nesrecomp::Opcode::BRK:
            ir = IRInstruction::make_brk(bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_flag_control(const Instruction6502& instr, ir::BasicBlock& block) {
    IRInstruction ir;
    uint8_t bank = instr.bank;
    uint16_t addr = instr.address;

    switch (instr.opcode_type) {
        case nesrecomp::Opcode::CLC:
            ir = IRInstruction::make_clc(bank, addr);
            break;
        case nesrecomp::Opcode::SEC:
            ir = IRInstruction::make_sec(bank, addr);
            break;
        case nesrecomp::Opcode::CLI:
            ir = IRInstruction::make_cli(bank, addr);
            break;
        case nesrecomp::Opcode::SEI:
            ir = IRInstruction::make_sei(bank, addr);
            break;
        case nesrecomp::Opcode::CLD:
            ir = IRInstruction::make_cld(bank, addr);
            break;
        case nesrecomp::Opcode::SED:
            ir = IRInstruction::make_sed(bank, addr);
            break;
        case nesrecomp::Opcode::CLV:
            ir = IRInstruction::make_clv(bank, addr);
            break;
        default:
            ir = IRInstruction::make_nop(bank, addr);
            break;
    }

    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

void IRBuilder::lower_misc(const Instruction6502& instr, ir::BasicBlock& block) {
    emit(block, IRInstruction::make_nop(instr.bank, instr.address), instr);
}

void IRBuilder::lower_unofficial(const Instruction6502& instr, ir::BasicBlock& block) {
    // Emit unofficial opcodes as NOP with a comment
    IRInstruction ir = IRInstruction::make_nop(instr.bank, instr.address);
    ir.cycles = instr.cycles;
    emit(block, ir, instr);
}

/* ============================================================================
 * IR Utilities
 * ========================================================================== */

const char* opcode_name(Opcode op) {
    switch (op) {
        case Opcode::NOP: return "NOP";
        case Opcode::LOAD_A_IMM: return "LOAD_A_IMM";
        case Opcode::LOAD_A_ADDR: return "LOAD_A_ADDR";
        case Opcode::LOAD_A_X: return "LOAD_A_X";
        case Opcode::LOAD_A_Y: return "LOAD_A_Y";
        case Opcode::LOAD_X_IMM: return "LOAD_X_IMM";
        case Opcode::LOAD_X_ADDR: return "LOAD_X_ADDR";
        case Opcode::LOAD_Y_IMM: return "LOAD_Y_IMM";
        case Opcode::LOAD_Y_ADDR: return "LOAD_Y_ADDR";
        case Opcode::STORE_A_ADDR: return "STORE_A_ADDR";
        case Opcode::STORE_A_X: return "STORE_A_X";
        case Opcode::STORE_A_Y: return "STORE_A_Y";
        case Opcode::STORE_X_ADDR: return "STORE_X_ADDR";
        case Opcode::STORE_Y_ADDR: return "STORE_Y_ADDR";
        case Opcode::TRANSFER_A_X: return "TRANSFER_A_X";
        case Opcode::TRANSFER_A_Y: return "TRANSFER_A_Y";
        case Opcode::TRANSFER_X_A: return "TRANSFER_X_A";
        case Opcode::TRANSFER_Y_A: return "TRANSFER_Y_A";
        case Opcode::TRANSFER_X_SP: return "TRANSFER_X_SP";
        case Opcode::TRANSFER_SP_X: return "TRANSFER_SP_X";
        case Opcode::PUSH_A: return "PUSH_A";
        case Opcode::PUSH_SR: return "PUSH_SR";
        case Opcode::PULL_A: return "PULL_A";
        case Opcode::PULL_SR: return "PULL_SR";
        case Opcode::ADC: return "ADC";
        case Opcode::SBC: return "SBC";
        case Opcode::AND: return "AND";
        case Opcode::ORA: return "ORA";
        case Opcode::EOR: return "EOR";
        case Opcode::BIT: return "BIT";
        case Opcode::CMP: return "CMP";
        case Opcode::CPX: return "CPX";
        case Opcode::CPY: return "CPY";
        case Opcode::ASL: return "ASL";
        case Opcode::LSR: return "LSR";
        case Opcode::ROL: return "ROL";
        case Opcode::ROR: return "ROR";
        case Opcode::INC: return "INC";
        case Opcode::DEC: return "DEC";
        case Opcode::INC_X: return "INC_X";
        case Opcode::DEC_X: return "DEC_X";
        case Opcode::INC_Y: return "INC_Y";
        case Opcode::DEC_Y: return "DEC_Y";
        case Opcode::JMP_ABS: return "JMP_ABS";
        case Opcode::JMP_IND: return "JMP_IND";
        case Opcode::JSR: return "JSR";
        case Opcode::RTS: return "RTS";
        case Opcode::RTI: return "RTI";
        case Opcode::BCC: return "BCC";
        case Opcode::BCS: return "BCS";
        case Opcode::BEQ: return "BEQ";
        case Opcode::BNE: return "BNE";
        case Opcode::BMI: return "BMI";
        case Opcode::BPL: return "BPL";
        case Opcode::BVC: return "BVC";
        case Opcode::BVS: return "BVS";
        case Opcode::CLC: return "CLC";
        case Opcode::SEC: return "SEC";
        case Opcode::CLI: return "CLI";
        case Opcode::SEI: return "SEI";
        case Opcode::CLD: return "CLD";
        case Opcode::SED: return "SED";
        case Opcode::CLV: return "CLV";
        case Opcode::BRK: return "BRK";
        case Opcode::LABEL: return "LABEL";
        default: return "???";
    }
}

std::string format_instruction(const IRInstruction& instr) {
    std::ostringstream ss;
    ss << opcode_name(instr.opcode);
    if (!instr.comment.empty()) {
        ss << " ; " << instr.comment;
    }
    return ss.str();
}

void dump_program(const Program& program, std::ostream& out) {
    out << "Program: " << program.rom_name << "\n";
    out << "Functions: " << program.functions.size() << "\n";
    out << "Blocks: " << program.blocks.size() << "\n";
}

} // namespace ir
} // namespace nesrecomp
