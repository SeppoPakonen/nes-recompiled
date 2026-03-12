/**
 * @file ir.h
 * @brief Intermediate Representation for 6502 (NES) instructions
 *
 * The IR layer decouples instruction semantics from code generation,
 * enabling optimization passes and future backend support (e.g., LLVM).
 */

#ifndef RECOMPILER_IR_H
#define RECOMPILER_IR_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace nesrecomp {
namespace ir {

/* ============================================================================
 * IR Opcodes (6502-specific)
 * ========================================================================== */

enum class Opcode : uint16_t {
    // === Data Movement - Load ===
    LOAD_A_IMM,         // LDA #imm - Load accumulator with immediate
    LOAD_A_ADDR,        // LDA addr - Load accumulator with memory
    LOAD_A_X,           // LDA addr,X - Load accumulator with indexed memory
    LOAD_A_Y,           // LDA addr,Y - Load accumulator with Y-indexed memory
    LOAD_A_IND_X,       // LDA (addr,X) - Load accumulator with indirect,X indexed memory
    LOAD_A_IND_Y,       // LDA (addr),Y - Load accumulator with indirect,Y indexed memory
    LOAD_X_IMM,         // LDX #imm - Load X with immediate
    LOAD_X_ADDR,        // LDX addr - Load X with memory
    LOAD_X_Y,           // LDX addr,Y - Load X with Y-indexed memory
    LOAD_Y_IMM,         // LDY #imm - Load Y with immediate
    LOAD_Y_ADDR,        // LDY addr - Load Y with memory
    LOAD_Y_X,           // LDY addr,X - Load Y with X-indexed memory

    // === Data Movement - Store ===
    STORE_A_ADDR,       // STA addr - Store accumulator to memory
    STORE_A_X,          // STA addr,X - Store accumulator to X-indexed memory
    STORE_A_Y,          // STA addr,Y - Store accumulator to Y-indexed memory
    STORE_A_IND_X,      // STA (addr,X) - Store accumulator to indirect,X indexed memory
    STORE_A_IND_Y,      // STA (addr),Y - Store accumulator to indirect,Y indexed memory
    STORE_X_ADDR,       // STX addr - Store X to memory
    STORE_X_Y,          // STX addr,Y - Store X to Y-indexed memory
    STORE_Y_ADDR,       // STY addr - Store Y to memory
    STORE_Y_X,          // STY addr,X - Store Y to X-indexed memory

    // === Data Movement - Transfer ===
    TRANSFER_A_X,       // TAX - Transfer A to X
    TRANSFER_A_Y,       // TAY - Transfer A to Y
    TRANSFER_X_A,       // TXA - Transfer X to A
    TRANSFER_Y_A,       // TYA - Transfer Y to A
    TRANSFER_X_SP,      // TXS - Transfer X to SP
    TRANSFER_SP_X,      // TSX - Transfer SP to X

    // === Data Movement - Stack ===
    PUSH_A,             // PHA - Push accumulator
    PUSH_SR,            // PHP - Push status register
    PULL_A,             // PLA - Pull accumulator
    PULL_SR,            // PLP - Pull status register

    // === ALU Operations - Arithmetic ===
    ADC,                // ADC - Add with carry
    SBC,                // SBC - Subtract with carry

    // === ALU Operations - Logical ===
    AND,                // AND - Logical AND
    ORA,                // ORA - Logical OR
    EOR,                // EOR - Logical XOR
    BIT,                // BIT - Bit test

    // === ALU Operations - Shift/Rotate ===
    ASL,                // ASL - Arithmetic shift left
    LSR,                // LSR - Logical shift right
    ROL,                // ROL - Rotate left
    ROR,                // ROR - Rotate right

    // === ALU Operations - Increment/Decrement ===
    INC,                // INC - Increment memory
    INC_A,              // INA - Increment accumulator (unofficial)
    INC_X,              // INX - Increment X
    INC_Y,              // INY - Increment Y
    DEC,                // DEC - Decrement memory
    DEC_A,              // DEA - Decrement accumulator (unofficial)
    DEC_X,              // DEX - Decrement X
    DEC_Y,              // DEY - Decrement Y

    // === ALU Operations - Compare ===
    CMP,                // CMP - Compare with A
    CPX,                // CPX - Compare with X
    CPY,                // CPY - Compare with Y

    // === Control Flow - Jumps ===
    JMP_ABS,            // JMP addr - Absolute jump
    JMP_IND,            // JMP (addr) - Indirect jump
    JSR,                // JSR addr - Jump to subroutine
    RTS,                // RTS - Return from subroutine
    RTI,                // RTI - Return from interrupt

    // === Control Flow - Branches ===
    BCC,                // BCC - Branch on carry clear
    BCS,                // BCS - Branch on carry set
    BEQ,                // BEQ - Branch on equal (Z set)
    BNE,                // BNE - Branch on not equal (Z clear)
    BMI,                // BMI - Branch on minus (N set)
    BPL,                // BPL - Branch on plus (N clear)
    BVC,                // BVC - Branch on overflow clear
    BVS,                // BVS - Branch on overflow set

    // === Control Flow - Special ===
    BRK,                // BRK - Break / software interrupt

    // === Flag Control ===
    CLC,                // CLC - Clear carry flag
    SEC,                // SEC - Set carry flag
    CLI,                // CLI - Clear interrupt disable
    SEI,                // SEI - Set interrupt disable
    CLD,                // CLD - Clear decimal mode
    SED,                // SED - Set decimal mode
    CLV,                // CLV - Clear overflow flag

    // === Special ===
    NOP,                // NOP - No operation

    // === Unofficial Opcodes ===
    // Load/Store unofficial
    LAX_IMM,            // LAX #imm - Load A and X with immediate
    LAX_ADDR,           // LAX addr - Load A and X with memory
    LAX_X,              // LAX addr,X - Load A and X with indexed memory
    SAX_ADDR,           // SAX addr - Store A and X to memory
    SAX_X,              // SAX addr,X - Store A and X to X-indexed memory
    SAX_Y,              // SAX addr,Y - Store A and X to Y-indexed memory
    LAS_ADDR,           // LAS addr,Y - Load A, X, SP with Y-indexed memory
    SHA_ADDR,           // SHA addr,Y - Store A and X with adjustment
    SHS_ADDR,           // SHS addr,Y - Store A and X to SP
    SHX_ADDR,           // SHX addr,Y - Store X with adjustment
    SHY_ADDR,           // SHY addr,X - Store Y with adjustment

    // Arithmetic/logical unofficial
    ALR,                // ALR - AND then LSR (immediate)
    ANC,                // ANC - AND then set carry (immediate)
    ANE,                // ANE - Transfer A to X then AND (immediate)
    ARR,                // ARR - AND then ROR (immediate)
    AXS,                // AXS - Subtract X without carry (immediate)
    DCP,                // DCP - Decrement and compare
    ISB,                // ISB - Increment and subtract with borrow
    RLA,                // RLA - Rotate left and AND
    RRA,                // RRA - Rotate right and ADC
    SLO,                // SLO - Shift left and OR
    SRE,                // SRE - Shift right and EOR
    SKB,                // SKB - Skip byte (NOP variant)
    IGN,                // IGN - Ignore (NOP variant)

    // === Meta ===
    LABEL,              // Label definition (pseudo-op)
    COMMENT,            // Debugging info (pseudo-op)
    SOURCE_LOC,         // Source location marker (pseudo-op)
};

/* ============================================================================
 * Operand Types
 * ========================================================================== */

enum class OperandType : uint8_t {
    NONE = 0,
    REG8,           // 8-bit register (A, X, Y)
    IMM8,           // 8-bit immediate
    IMM16,          // 16-bit immediate
    OFFSET,         // Signed 8-bit offset (for branches)
    ADDR,           // 16-bit address
    COND,           // Condition code (N, V, C, Z)
    BIT_IDX,        // Bit index 0-7
    BANK,           // ROM bank number
    MEM_IMM16,      // Memory at [imm16]
    MEM_ZPG,        // Zero page memory [imm8]
    MEM_ZPG_X,      // Zero page X indexed [imm8+X]
    MEM_ZPG_Y,      // Zero page Y indexed [imm8+Y]
    MEM_ABS_X,      // Absolute X indexed [imm16+X]
    MEM_ABS_Y,      // Absolute Y indexed [imm16+Y]
    MEM_IND,        // Indirect [(imm16)]
    MEM_IND_X,      // Indirect X indexed [(imm8+X)]
    MEM_IND_Y,      // Indirect Y indexed [(imm8)+Y]
    LABEL_REF,      // Reference to a label
};

/* ============================================================================
 * 6502 Registers
 * ========================================================================== */

enum class Reg8_6502 : uint8_t {
    A = 0,  // Accumulator
    X = 1,  // Index register X
    Y = 2,  // Index register Y
    SP = 3, // Stack pointer (8-bit, high byte of address)
};

/* ============================================================================
 * 6502 Condition Codes
 * ========================================================================== */

enum class Cond6502 : uint8_t {
    N = 0,    // Negative flag set
    P = 1,    // Positive flag clear (N=0)
    Z = 2,    // Zero flag set
    NZ = 3,   // Zero flag clear
    C = 4,    // Carry flag set
    NC = 5,   // Carry flag clear
    V = 6,    // Overflow flag set
    NV = 7,   // Overflow flag clear
};

/* ============================================================================
 * IR Operand
 * ========================================================================== */

struct Operand {
    OperandType type = OperandType::NONE;
    uint8_t bank = 0; // ROM bank context for this operand (0 for fixed, 1-N for switched)

    union {
        uint8_t reg8;           // Register index (0=A, 1=X, 2=Y, 3=SP)
        uint8_t imm8;
        uint16_t imm16;
        int8_t offset;
        uint8_t bit_idx;
        uint8_t bank;
        uint8_t condition;      // Condition code for branches
        uint32_t label_id;
    } value = {0};

    // Constructors
    static Operand none() { return Operand{}; }
    static Operand reg8(uint8_t r);
    static Operand imm8(uint8_t v);
    static Operand imm16(uint16_t v);
    static Operand offset(int8_t o);
    static Operand condition(uint8_t c);
    static Operand bit_idx(uint8_t b);
    static Operand mem_imm16(uint16_t addr);
    static Operand mem_zpg(uint8_t addr);
    static Operand mem_zpg_x(uint8_t addr);
    static Operand mem_zpg_y(uint8_t addr);
    static Operand mem_abs_x(uint16_t addr);
    static Operand mem_abs_y(uint16_t addr);
    static Operand mem_ind(uint16_t addr);
    static Operand mem_ind_x(uint8_t addr);
    static Operand mem_ind_y(uint8_t addr);
    static Operand label(uint32_t id);
};

/* ============================================================================
 * Flag Effects (6502-specific)
 * ========================================================================== */

struct FlagEffects {
    // Which flags are affected
    bool affects_n : 1;     // Negative flag (bit 7)
    bool affects_v : 1;     // Overflow flag (bit 6)
    bool affects_z : 1;     // Zero flag (bit 1)
    bool affects_c : 1;     // Carry flag (bit 0)
    bool affects_d : 1;     // Decimal mode flag (bit 3)
    bool affects_i : 1;     // Interrupt disable flag (bit 2)

    // For flags that are set to a fixed value
    bool fixed_n : 1;       // If affects_n && fixed_n, N = n_value
    bool fixed_v : 1;
    bool fixed_z : 1;
    bool fixed_c : 1;
    bool fixed_d : 1;
    bool fixed_i : 1;

    bool n_value : 1;
    bool v_value : 1;
    bool z_value : 1;
    bool c_value : 1;
    bool d_value : 1;
    bool i_value : 1;

    // Factory methods for common flag patterns
    static FlagEffects none();          // No flags affected
    static FlagEffects nzvc();          // N, Z, V, C affected (ADC, SBC)
    static FlagEffects nz();            // N, Z affected (LDA, LDX, LDY, transfers)
    static FlagEffects nzc();           // N, Z, C affected (shifts, rotates)
    static FlagEffects nzv();           // N, Z, V affected (BIT)
    static FlagEffects c_only();        // Only C affected (CLC, SEC, etc.)
    static FlagEffects d_only();        // Only D affected (CLD, SED)
    static FlagEffects i_only();        // Only I affected (CLI, SEI)
    static FlagEffects v_only();        // Only V affected (CLV)
};

/* ============================================================================
 * IR Instruction
 * ========================================================================== */

struct IRInstruction {
    Opcode opcode;
    Operand dst;
    Operand src;
    Operand extra;          // For 3-operand ops or additional info

    // Source location tracking
    uint8_t source_bank = 0;
    uint16_t source_address = 0;
    bool has_source_location = false;

    // Cycle cost
    uint8_t cycles = 0;
    uint8_t cycles_branch_taken = 0;

    // Flag effects
    FlagEffects flags = FlagEffects::none();

    // Debug info
    std::string comment;

    // Factory methods for 6502 instructions
    static IRInstruction make_nop(uint8_t bank, uint16_t addr);
    static IRInstruction make_label(uint32_t label_id);
    static IRInstruction make_comment(const std::string& text);

    // Load instructions
    static IRInstruction make_load_a_imm(uint8_t imm, uint8_t bank, uint16_t addr);
    static IRInstruction make_load_a_addr(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_load_a_x(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_load_a_y(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_load_a_ind_x(uint8_t zp_addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_load_a_ind_y(uint8_t zp_addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_load_x_imm(uint8_t imm, uint8_t bank, uint16_t addr);
    static IRInstruction make_load_x_addr(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_load_y_imm(uint8_t imm, uint8_t bank, uint16_t addr);
    static IRInstruction make_load_y_addr(uint16_t addr, uint8_t bank, uint16_t src_addr);

    // Store instructions
    static IRInstruction make_store_a_addr(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_store_a_x(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_store_a_y(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_store_a_ind_x(uint8_t zp_addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_store_a_ind_y(uint8_t zp_addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_store_x_addr(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_store_y_addr(uint16_t addr, uint8_t bank, uint16_t src_addr);

    // Transfer instructions
    static IRInstruction make_transfer_a_x(uint8_t bank, uint16_t addr);
    static IRInstruction make_transfer_a_y(uint8_t bank, uint16_t addr);
    static IRInstruction make_transfer_x_a(uint8_t bank, uint16_t addr);
    static IRInstruction make_transfer_y_a(uint8_t bank, uint16_t addr);
    static IRInstruction make_transfer_x_sp(uint8_t bank, uint16_t addr);
    static IRInstruction make_transfer_sp_x(uint8_t bank, uint16_t addr);

    // Stack instructions
    static IRInstruction make_push_a(uint8_t bank, uint16_t addr);
    static IRInstruction make_push_sr(uint8_t bank, uint16_t addr);
    static IRInstruction make_pull_a(uint8_t bank, uint16_t addr);
    static IRInstruction make_pull_sr(uint8_t bank, uint16_t addr);

    // ALU instructions
    static IRInstruction make_adc(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_sbc(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_and(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_ora(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_eor(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_bit(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_cmp(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_cpx(uint8_t src, uint8_t bank, uint16_t addr);
    static IRInstruction make_cpy(uint8_t src, uint8_t bank, uint16_t addr);

    // Shift/Rotate instructions
    static IRInstruction make_asl(uint8_t bank, uint16_t addr);
    static IRInstruction make_lsr(uint8_t bank, uint16_t addr);
    static IRInstruction make_rol(uint8_t bank, uint16_t addr);
    static IRInstruction make_ror(uint8_t bank, uint16_t addr);

    // Increment/Decrement
    static IRInstruction make_inc(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_dec(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_inc_x(uint8_t bank, uint16_t addr);
    static IRInstruction make_dec_x(uint8_t bank, uint16_t addr);
    static IRInstruction make_inc_y(uint8_t bank, uint16_t addr);
    static IRInstruction make_dec_y(uint8_t bank, uint16_t addr);

    // Jump instructions
    static IRInstruction make_jmp_abs(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_jmp_ind(uint16_t addr, uint8_t bank, uint16_t src_addr);
    static IRInstruction make_jsr(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_rts(uint8_t bank, uint16_t addr);
    static IRInstruction make_rti(uint8_t bank, uint16_t addr);

    // Branch instructions
    static IRInstruction make_bcc(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_bcs(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_beq(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_bne(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_bmi(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_bpl(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_bvc(uint32_t label_id, uint8_t bank, uint16_t addr);
    static IRInstruction make_bvs(uint32_t label_id, uint8_t bank, uint16_t addr);

    // Flag control instructions
    static IRInstruction make_clc(uint8_t bank, uint16_t addr);
    static IRInstruction make_sec(uint8_t bank, uint16_t addr);
    static IRInstruction make_cli(uint8_t bank, uint16_t addr);
    static IRInstruction make_sei(uint8_t bank, uint16_t addr);
    static IRInstruction make_cld(uint8_t bank, uint16_t addr);
    static IRInstruction make_sed(uint8_t bank, uint16_t addr);
    static IRInstruction make_clv(uint8_t bank, uint16_t addr);

    // Special
    static IRInstruction make_brk(uint8_t bank, uint16_t addr);
};

/* ============================================================================
 * Basic Block
 * ========================================================================== */

struct BasicBlock {
    uint32_t id;
    std::string label;
    std::vector<IRInstruction> instructions;
    
    // Control flow
    std::vector<uint32_t> successors;
    std::vector<uint32_t> predecessors;
    
    // Bank info
    uint8_t bank = 0;
    uint16_t start_address = 0;
    uint16_t end_address = 0;
    
    // Flags
    bool is_entry = false;
    bool is_interrupt_handler = false;
    bool is_reachable = false;
};

/* ============================================================================
 * Function
 * ========================================================================== */

struct Function {
    std::string name;
    uint8_t bank = 0;
    uint16_t entry_address = 0;
    
    std::vector<uint32_t> block_ids;
    
    bool is_interrupt_handler = false;
    bool is_entry_point = false;
    bool crosses_banks = false;
};

/* ============================================================================
 * IR Program
 * ========================================================================== */

struct Program {
    std::string rom_name;
    
    // All basic blocks
    std::map<uint32_t, BasicBlock> blocks;
    uint32_t next_block_id = 0;
    
    // Functions
    std::map<std::string, Function> functions;
    
    // Labels (for cross-referencing)
    std::map<uint32_t, std::string> labels;         // id -> name
    std::map<std::string, uint32_t> label_by_name;  // name -> id
    uint32_t next_label_id = 0;
    
    // ROM info
    uint8_t mbc_type = 0;
    uint16_t rom_bank_count = 0;
    
    // Entry points
    uint16_t main_entry = 0x100;
    std::vector<uint16_t> interrupt_vectors;
    
    // Create a new block
    uint32_t create_block(uint8_t bank, uint16_t addr);
    
    // Create/lookup labels
    uint32_t create_label(const std::string& name);
    uint32_t get_or_create_label(const std::string& name);
    std::string get_label_name(uint32_t id) const;
    
    // Generate unique label for address
    std::string make_address_label(uint8_t bank, uint16_t addr) const;
    std::string make_function_name(uint8_t bank, uint16_t addr) const;
};

/* ============================================================================
 * IR Utilities
 * ========================================================================== */

/**
 * @brief Get opcode name for debugging
 */
const char* opcode_name(Opcode op);

/**
 * @brief Print IR instruction for debugging
 */
std::string format_instruction(const IRInstruction& instr);

/**
 * @brief Print entire program for debugging
 */
void dump_program(const Program& program, std::ostream& out);

} // namespace ir
} // namespace nesrecomp

#endif // RECOMPILER_IR_H
