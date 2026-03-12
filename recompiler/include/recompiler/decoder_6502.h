/**
 * @file decoder_6502.h
 * @brief 6502 instruction decoder
 *
 * Decodes all 256 opcode slots of the 6502 CPU.
 * Includes 56 official opcodes and 54 unofficial opcodes.
 */

#ifndef RECOMPILER_DECODER_6502_H
#define RECOMPILER_DECODER_6502_H

#include <cstdint>
#include <string>
#include <vector>

namespace gbrecomp {

/* ============================================================================
 * Register Definitions
 * ========================================================================== */

/**
 * @brief 8-bit registers
 * 
 * 6502 has only three general-purpose 8-bit registers:
 * - A: Accumulator
 * - X: Index register X
 * - Y: Index register Y
 */
enum class Reg8_6502 : uint8_t {
    A = 0,  ///< Accumulator
    X = 1,  ///< Index register X
    Y = 2   ///< Index register Y
};

/* ============================================================================
 * Addressing Modes
 * ========================================================================== */

/**
 * @brief 6502 addressing modes
 * 
 * Matches the NES emulator's AddressMode enum for compatibility.
 */
enum class AddressMode : uint8_t {
    NONE,       ///< No operand (internal use)
    IMPL,       ///< Implied (e.g., TAX, RTS)
    ACC,        ///< Accumulator (e.g., ASL A)
    REL,        ///< Relative (e.g., BEQ label)
    IMT,        ///< Immediate (e.g., LDA #$42)
    ZPG,        ///< Zero Page (e.g., LDA $42)
    ZPG_X,      ///< Zero Page X (e.g., LDA $42,X)
    ZPG_Y,      ///< Zero Page Y (e.g., LDA $42,Y)
    ABS,        ///< Absolute (e.g., LDA $4242)
    ABS_X,      ///< Absolute X (e.g., LDA $4242,X)
    ABS_Y,      ///< Absolute Y (e.g., LDA $4242,Y)
    IND,        ///< Indirect (e.g., JMP ($4242))
    IND_IDX,    ///< Indirect Indexed (e.g., LDA ($42),Y)
    IDX_IND     ///< Indexed Indirect (e.g., LDA ($42,X))
};

/* ============================================================================
 * Opcode Definitions
 * ========================================================================== */

/**
 * @brief 6502 opcodes
 * 
 * Includes official and unofficial opcodes.
 * Unofficial opcodes are marked with their common names.
 */
enum class Opcode : uint8_t {
    // Official opcodes (56)
    ADC,    ///< Add with Carry
    AND,    ///< And (with accumulator)
    ASL,    ///< Arithmetic Shift Left
    BCC,    ///< Branch on Carry Clear
    BCS,    ///< Branch on Carry Set
    BEQ,    ///< Branch on Equal (zero set)
    BIT,    ///< Bit Test
    BMI,    ///< Branch on Minus (negative set)
    BNE,    ///< Branch on Not Equal (zero clear)
    BPL,    ///< Branch on Plus (negative clear)
    BRK,    ///< Break / Interrupt
    BVC,    ///< Branch on Overflow Clear
    BVS,    ///< Branch on Overflow Set
    CLC,    ///< Clear Carry
    CLD,    ///< Clear Decimal
    CLI,    ///< Clear Interrupt Disable
    CLV,    ///< Clear Overflow
    CMP,    ///< Compare (with accumulator)
    CPX,    ///< Compare with X
    CPY,    ///< Compare with Y
    DEC,    ///< Decrement
    DEX,    ///< Decrement X
    DEY,    ///< Decrement Y
    EOR,    ///< Exclusive Or (with accumulator)
    INC,    ///< Increment
    INX,    ///< Increment X
    INY,    ///< Increment Y
    JMP,    ///< Jump
    JSR,    ///< Jump Subroutine
    LDA,    ///< Load Accumulator
    LDX,    ///< Load X
    LDY,    ///< Load Y
    LSR,    ///< Logical Shift Right
    NOP,    ///< No Operation
    ORA,    ///< Or with Accumulator
    PHA,    ///< Push Accumulator
    PHP,    ///< Push Processor Status (SR)
    PLA,    ///< Pull Accumulator
    PLP,    ///< Pull Processor Status (SR)
    ROL,    ///< Rotate Left
    ROR,    ///< Rotate Right
    RTI,    ///< Return from Interrupt
    RTS,    ///< Return from Subroutine
    SBC,    ///< Subtract with Carry
    SEC,    ///< Set Carry
    SED,    ///< Set Decimal
    SEI,    ///< Set Interrupt Disable
    STA,    ///< Store Accumulator
    STX,    ///< Store X
    STY,    ///< Store Y
    TAX,    ///< Transfer Accumulator to X
    TAY,    ///< Transfer Accumulator to Y
    TSX,    ///< Transfer Stack Pointer to X
    TXA,    ///< Transfer X to Accumulator
    TXS,    ///< Transfer X to Stack Pointer
    TYA,    ///< Transfer Y to Accumulator

    // Unofficial opcodes (54)
    ALR,    ///< ASR - And then Logical Shift Right
    ANC,    ///< And then set Carry
    ANE,    ///< XAA - Transfer A to X then And
    ARR,    ///< And then Rotate Right
    AXS,    ///< SBX, SAX - Subtract X (without carry)
    LAX,    ///< LXA - Load A and X
    LAS,    ///< LAR - Load A, X, and SP
    SAX,    ///< AXS, AAX - Store A and X
    SHA,    ///< AHX - Store A and X with adjustment
    SHS,    ///< XAS, TAS - Transfer X and A to SP
    SHX,    ///< A11, SXA, XAS - Store X with adjustment
    SHY,    ///< A11, SYA, SAY - Store Y with adjustment

    DCP,    ///< DCM - Decrement and Compare
    ISB,    ///< ISC, INS - Increment and Subtract with Borrow
    RLA,    ///< Rotate Left and And
    RRA,    ///< Rotate Right and Add
    SLO,    ///< ASO - Shift Left and Or
    SRE,    ///< LSE - Shift Right and Exclusive Or

    SKB,    ///< Skip Byte (various NOP variants)
    IGN,    ///< Ignore (various NOP variants)

    // Special
    UNDEFINED,  ///< Undefined/Invalid opcode
    NIL_OP      ///< No operation (placeholder for unused slots)
};

/* ============================================================================
 * Instruction Structure
 * ========================================================================== */

/**
 * @brief Decoded 6502 instruction with all operand information
 */
struct Instruction6502 {
    // Location
    uint16_t address;           ///< Address in ROM
    uint8_t bank;               ///< ROM bank (0 for fixed bank)

    // Raw bytes
    uint8_t opcode;             ///< Primary opcode byte

    // Decoded type
    Opcode opcode_type;         ///< Decoded opcode type
    AddressMode mode;           ///< Addressing mode

    // Size and timing
    uint8_t length;             ///< 1, 2, or 3 bytes
    uint8_t cycles;             ///< Base cycle count
    uint8_t cycles_branch;      ///< Cycles if branch taken (for conditionals)

    // Operands
    Reg8_6502 reg8_dst;              ///< Destination 8-bit register
    Reg8_6502 reg8_src;              ///< Source 8-bit register
    uint8_t imm8;               ///< 8-bit immediate
    uint16_t imm16;             ///< 16-bit immediate
    int8_t offset;              ///< Signed offset for relative branches

    // Control flow flags
    bool is_jump;               ///< JMP, Bxx
    bool is_call;               ///< JSR
    bool is_return;             ///< RTS, RTI
    bool is_conditional;        ///< Has condition code (Bxx)
    bool is_terminator;         ///< Ends basic block

    // Memory access flags
    bool reads_memory;          ///< Reads from memory
    bool writes_memory;         ///< Writes to memory

    // Flag effects
    struct {
        bool affects_n : 1;     ///< Negative flag
        bool affects_z : 1;     ///< Zero flag
        bool affects_c : 1;     ///< Carry flag
        bool affects_v : 1;     ///< Overflow flag
        bool affects_d : 1;     ///< Decimal flag
        bool affects_i : 1;     ///< Interrupt disable flag
    } flag_effects;

    /**
     * @brief Get disassembly string
     */
    std::string disassemble() const;

    /**
     * @brief Get raw bytes as hex string
     */
    std::string bytes_hex() const;
};

// Forward declaration
class ROM;

/* ============================================================================
 * Decoder Class
 * ========================================================================== */

/**
 * @brief 6502 instruction decoder
 */
class Decoder6502 {
public:
    explicit Decoder6502(const ROM& rom);

    Instruction6502 decode(uint32_t full_addr) const;
    Instruction6502 decode(uint16_t addr, uint8_t bank) const;

private:
    uint16_t read_u16(uint16_t addr, uint8_t bank) const;

    const ROM& rom_;
};

/* ============================================================================
 * Decoder Interface
 * ========================================================================== */

/**
 * @brief Decode a single instruction at the given address
 *
 * @param rom ROM data
 * @param rom_size ROM size
 * @param address Address to decode at
 * @param bank Current ROM bank (0 for fixed region)
 * @return Decoded instruction
 */

/**
 * @brief Decode all instructions in a ROM bank
 *
 * Performs linear sweep decoding. For accurate results,
 * combine with control flow analysis.
 *
 * @param rom ROM data
 * @param bank Bank to decode (0 for bank 0, 1+ for switchable)
 * @return Vector of decoded instructions
 */
std::vector<Instruction6502> decode_bank_6502(const ROM& rom, uint8_t bank = 0);

/**
 * @brief Get cycle count for an instruction
 *
 * @param instr Instruction
 * @param branch_taken Whether branch was taken (for conditionals)
 * @return Cycle count
 */
uint8_t get_cycle_count_6502(const Instruction6502& instr, bool branch_taken = false);

/* ============================================================================
 * Disassembly
 * ========================================================================== */

/**
 * @brief Disassemble instruction to string
 */
std::string disassemble_6502(const Instruction6502& instr);

/**
 * @brief Get opcode name
 */
const char* opcode_name(Opcode op);

/**
 * @brief Get addressing mode name
 */
const char* address_mode_name(AddressMode mode);

/**
 * @brief Get register name
 */
const char* reg8_name_6502(Reg8_6502 reg);

} // namespace gbrecomp

#endif // RECOMPILER_DECODER_6502_H
