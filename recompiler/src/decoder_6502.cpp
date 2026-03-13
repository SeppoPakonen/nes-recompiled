/**
 * @file decoder_6502.cpp
 * @brief 6502 instruction decoder implementation
 *
 * Decodes all 256 opcode slots of the 6502 CPU.
 * Reference: NES emulator instruction lookup table at cpu6502.h:instructionLookup[256]
 */

#include "recompiler/decoder_6502.h"
#include "recompiler/rom.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <array>

namespace nesrecomp {

/* ============================================================================
 * Helper Tables
 * ========================================================================== */

// Register name lookup
static const char* REG8_NAMES[] = {
    "A", "X", "Y"
};

// Opcode name lookup
static const char* OPCODE_NAMES[] = {
    // Official
    "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI",
    "BNE", "BPL", "BRK", "BVC", "BVS", "CLC", "CLD", "CLI",
    "CLV", "CMP", "CPX", "CPY", "DEC", "DEX", "DEY", "EOR",
    "INC", "INX", "INY", "JMP", "JSR", "LDA", "LDX", "LDY",
    "LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL",
    "ROR", "RTI", "RTS", "SBC", "SEC", "SED", "SEI", "STA",
    "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA",
    // Unofficial
    "ALR", "ANC", "ANE", "ARR", "AXS", "LAX", "LAS", "SAX",
    "SHA", "SHS", "SHX", "SHY", "DCP", "ISB", "RLA", "RRA",
    "SLO", "SRE", "SKB", "IGN",
    // Special
    "UNDEFINED", "NIL_OP"
};

// Addressing mode name lookup
static const char* MODE_NAMES[] = {
    "NONE", "IMPL", "ACC", "REL", "IMT", "ZPG", "ZPG_X", "ZPG_Y",
    "ABS", "ABS_X", "ABS_Y", "IND", "IND_IDX", "IDX_IND"
};

// Cycle count table indexed by addressing mode
// Values are base cycles; some instructions have special handling
static constexpr uint8_t CYCLES_BY_MODE[14] = {
    0,  // NONE
    2,  // IMPL - varies by instruction
    2,  // ACC
    2,  // REL - +1 if branch taken, +1 if page crossed
    2,  // IMT
    3,  // ZPG
    4,  // ZPG_X
    4,  // ZPG_Y
    4,  // ABS
    4,  // ABS_X - +1 if page crossed
    4,  // ABS_Y - +1 if page crossed
    5,  // IND
    5,  // IND_IDX
    6   // IDX_IND
};

/* ============================================================================
 * Instruction Lookup Table
 * 
 * Based on NES emulator's instructionLookup[256]
 * Format: {Opcode, AddressMode}
 * NIL_OP marks unused/undefined opcode slots
 * ========================================================================== */

struct OpcodeEntry {
    Opcode op;
    AddressMode mode;
};

static constexpr OpcodeEntry INSTRUCTION_LOOKUP[256] = {
    // 0x00-0x0F
    {Opcode::BRK,  AddressMode::IMPL},    {Opcode::ORA,  AddressMode::IDX_IND}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::SLO,  AddressMode::IDX_IND},
    {Opcode::NOP,  AddressMode::ZPG},     {Opcode::ORA,  AddressMode::ZPG},     {Opcode::ASL,  AddressMode::ZPG},     {Opcode::SLO,  AddressMode::ZPG},
    {Opcode::PHP,  AddressMode::IMPL},    {Opcode::ORA,  AddressMode::IMT},     {Opcode::ASL,  AddressMode::ACC},     {Opcode::ANC,  AddressMode::IMT},
    {Opcode::NOP,  AddressMode::ABS},     {Opcode::ORA,  AddressMode::ABS},     {Opcode::ASL,  AddressMode::ABS},     {Opcode::SLO,  AddressMode::ABS},

    // 0x10-0x1F
    {Opcode::BPL,  AddressMode::REL},     {Opcode::ORA,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::SLO,  AddressMode::IND_IDX},
    {Opcode::NOP,  AddressMode::ZPG_X},   {Opcode::ORA,  AddressMode::ZPG_X},   {Opcode::ASL,  AddressMode::ZPG_X},   {Opcode::SLO,  AddressMode::ZPG_X},
    {Opcode::CLC,  AddressMode::IMPL},    {Opcode::ORA,  AddressMode::ABS_Y},   {Opcode::NOP,  AddressMode::IMPL},   {Opcode::SLO,  AddressMode::ABS_Y},
    {Opcode::NOP,  AddressMode::ABS_X},   {Opcode::ORA,  AddressMode::ABS_X},   {Opcode::ASL,  AddressMode::ABS_X},   {Opcode::SLO,  AddressMode::ABS_X},

    // 0x20-0x2F
    {Opcode::JSR,  AddressMode::ABS},     {Opcode::AND,  AddressMode::IDX_IND}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::RLA,  AddressMode::IDX_IND},
    {Opcode::BIT,  AddressMode::ZPG},     {Opcode::AND,  AddressMode::ZPG},     {Opcode::ROL,  AddressMode::ZPG},     {Opcode::RLA,  AddressMode::ZPG},
    {Opcode::PLP,  AddressMode::IMPL},    {Opcode::AND,  AddressMode::IMT},     {Opcode::ROL,  AddressMode::ACC},     {Opcode::ANC,  AddressMode::IMT},
    {Opcode::BIT,  AddressMode::ABS},     {Opcode::AND,  AddressMode::ABS},     {Opcode::ROL,  AddressMode::ABS},     {Opcode::RLA,  AddressMode::ABS},

    // 0x30-0x3F
    {Opcode::BMI,  AddressMode::REL},     {Opcode::AND,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::RLA,  AddressMode::IND_IDX},
    {Opcode::NOP,  AddressMode::ZPG_X},   {Opcode::AND,  AddressMode::ZPG_X},   {Opcode::ROL,  AddressMode::ZPG_X},   {Opcode::RLA,  AddressMode::ZPG_X},
    {Opcode::SEC,  AddressMode::IMPL},    {Opcode::AND,  AddressMode::ABS_Y},   {Opcode::NOP,  AddressMode::IMPL},   {Opcode::RLA,  AddressMode::ABS_Y},
    {Opcode::NOP,  AddressMode::ABS_X},   {Opcode::AND,  AddressMode::ABS_X},   {Opcode::ROL,  AddressMode::ABS_X},   {Opcode::RLA,  AddressMode::ABS_X},

    // 0x40-0x4F
    {Opcode::RTI,  AddressMode::IMPL},    {Opcode::EOR,  AddressMode::IDX_IND}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::SRE,  AddressMode::IDX_IND},
    {Opcode::NOP,  AddressMode::ZPG},     {Opcode::EOR,  AddressMode::ZPG},     {Opcode::LSR,  AddressMode::ZPG},     {Opcode::SRE,  AddressMode::ZPG},
    {Opcode::PHA,  AddressMode::IMPL},    {Opcode::EOR,  AddressMode::IMT},     {Opcode::LSR,  AddressMode::ACC},     {Opcode::ALR,  AddressMode::IMT},
    {Opcode::JMP,  AddressMode::ABS},     {Opcode::EOR,  AddressMode::ABS},     {Opcode::LSR,  AddressMode::ABS},     {Opcode::SRE,  AddressMode::ABS},

    // 0x50-0x5F
    {Opcode::BVC,  AddressMode::REL},     {Opcode::EOR,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::SRE,  AddressMode::IND_IDX},
    {Opcode::NOP,  AddressMode::ZPG_X},   {Opcode::EOR,  AddressMode::ZPG_X},   {Opcode::LSR,  AddressMode::ZPG_X},   {Opcode::SRE,  AddressMode::ZPG_X},
    {Opcode::CLI,  AddressMode::IMPL},    {Opcode::EOR,  AddressMode::ABS_Y},   {Opcode::NOP,  AddressMode::IMPL},   {Opcode::SRE,  AddressMode::ABS_Y},
    {Opcode::NOP,  AddressMode::ABS_X},   {Opcode::EOR,  AddressMode::ABS_X},   {Opcode::LSR,  AddressMode::ABS_X},   {Opcode::SRE,  AddressMode::ABS_X},

    // 0x60-0x6F
    {Opcode::RTS,  AddressMode::IMPL},    {Opcode::ADC,  AddressMode::IDX_IND}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::RRA,  AddressMode::IDX_IND},
    {Opcode::NOP,  AddressMode::ZPG},     {Opcode::ADC,  AddressMode::ZPG},     {Opcode::ROR,  AddressMode::ZPG},     {Opcode::RRA,  AddressMode::ZPG},
    {Opcode::PLA,  AddressMode::IMPL},    {Opcode::ADC,  AddressMode::IMT},     {Opcode::ROR,  AddressMode::ACC},     {Opcode::ARR,  AddressMode::IMT},
    {Opcode::JMP,  AddressMode::IND},     {Opcode::ADC,  AddressMode::ABS},     {Opcode::ROR,  AddressMode::ABS},     {Opcode::RRA,  AddressMode::ABS},

    // 0x70-0x7F
    {Opcode::BVS,  AddressMode::REL},     {Opcode::ADC,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::RRA,  AddressMode::IND_IDX},
    {Opcode::NOP,  AddressMode::ZPG_X},   {Opcode::ADC,  AddressMode::ZPG_X},   {Opcode::ROR,  AddressMode::ZPG_X},   {Opcode::RRA,  AddressMode::ZPG_X},
    {Opcode::SEI,  AddressMode::IMPL},    {Opcode::ADC,  AddressMode::ABS_Y},   {Opcode::NOP,  AddressMode::IMPL},   {Opcode::RRA,  AddressMode::ABS_Y},
    {Opcode::NOP,  AddressMode::ABS_X},   {Opcode::ADC,  AddressMode::ABS_X},   {Opcode::ROR,  AddressMode::ABS_X},   {Opcode::RRA,  AddressMode::ABS_X},

    // 0x80-0x8F
    {Opcode::NOP,  AddressMode::IMT},     {Opcode::STA,  AddressMode::IDX_IND}, {Opcode::NOP,  AddressMode::IMT},   {Opcode::SAX,  AddressMode::IDX_IND},
    {Opcode::STY,  AddressMode::ZPG},     {Opcode::STA,  AddressMode::ZPG},     {Opcode::STX,  AddressMode::ZPG},   {Opcode::SAX,  AddressMode::ZPG},
    {Opcode::DEY,  AddressMode::IMPL},    {Opcode::NOP,  AddressMode::IMT},     {Opcode::TXA,  AddressMode::IMPL},  {Opcode::ANE,  AddressMode::IMT},
    {Opcode::STY,  AddressMode::ABS},     {Opcode::STA,  AddressMode::ABS},     {Opcode::STX,  AddressMode::ABS},   {Opcode::SAX,  AddressMode::ABS},

    // 0x90-0x9F
    {Opcode::BCC,  AddressMode::REL},     {Opcode::STA,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::SHA,  AddressMode::IND_IDX},
    {Opcode::STY,  AddressMode::ZPG_X},   {Opcode::STA,  AddressMode::ZPG_X},   {Opcode::STX,  AddressMode::ZPG_Y}, {Opcode::SAX,  AddressMode::ZPG_Y},
    {Opcode::TYA,  AddressMode::IMPL},    {Opcode::STA,  AddressMode::ABS_Y},   {Opcode::TXS,  AddressMode::IMPL},  {Opcode::SHS,  AddressMode::ABS_Y},
    {Opcode::SHY,  AddressMode::ABS_X},   {Opcode::STA,  AddressMode::ABS_X},   {Opcode::SHX,  AddressMode::ABS_Y}, {Opcode::SHA,  AddressMode::ABS_Y},

    // 0xA0-0xAF
    {Opcode::LDY,  AddressMode::IMT},     {Opcode::LDA,  AddressMode::IDX_IND}, {Opcode::LDX,  AddressMode::IMT},   {Opcode::LAX,  AddressMode::IDX_IND},
    {Opcode::LDY,  AddressMode::ZPG},     {Opcode::LDA,  AddressMode::ZPG},     {Opcode::LDX,  AddressMode::ZPG},   {Opcode::LAX,  AddressMode::ZPG},
    {Opcode::TAY,  AddressMode::IMPL},    {Opcode::LDA,  AddressMode::IMT},     {Opcode::TAX,  AddressMode::IMPL},  {Opcode::LAX,  AddressMode::IMT},
    {Opcode::LDY,  AddressMode::ABS},     {Opcode::LDA,  AddressMode::ABS},     {Opcode::LDX,  AddressMode::ABS},   {Opcode::LAX,  AddressMode::ABS},

    // 0xB0-0xBF
    {Opcode::BCS,  AddressMode::REL},     {Opcode::LDA,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::LAX,  AddressMode::IND_IDX},
    {Opcode::LDY,  AddressMode::ZPG_X},   {Opcode::LDA,  AddressMode::ZPG_X},   {Opcode::LDX,  AddressMode::ZPG_Y}, {Opcode::LAX,  AddressMode::ZPG_Y},
    {Opcode::CLV,  AddressMode::IMPL},    {Opcode::LDA,  AddressMode::ABS_Y},   {Opcode::TSX,  AddressMode::IMPL},  {Opcode::LAS,  AddressMode::ABS_Y},
    {Opcode::LDY,  AddressMode::ABS_X},   {Opcode::LDA,  AddressMode::ABS_X},   {Opcode::LDX,  AddressMode::ABS_Y}, {Opcode::LAX,  AddressMode::ABS_Y},

    // 0xC0-0xCF
    {Opcode::CPY,  AddressMode::IMT},     {Opcode::CMP,  AddressMode::IDX_IND}, {Opcode::NOP,  AddressMode::IMT},   {Opcode::DCP,  AddressMode::IDX_IND},
    {Opcode::CPY,  AddressMode::ZPG},     {Opcode::CMP,  AddressMode::ZPG},     {Opcode::DEC,  AddressMode::ZPG},   {Opcode::DCP,  AddressMode::ZPG},
    {Opcode::INY,  AddressMode::IMPL},    {Opcode::CMP,  AddressMode::IMT},     {Opcode::DEX,  AddressMode::IMPL},  {Opcode::AXS,  AddressMode::IMT},
    {Opcode::CPY,  AddressMode::ABS},     {Opcode::CMP,  AddressMode::ABS},     {Opcode::DEC,  AddressMode::ABS},   {Opcode::DCP,  AddressMode::ABS},

    // 0xD0-0xDF
    {Opcode::BNE,  AddressMode::REL},     {Opcode::CMP,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::DCP,  AddressMode::IND_IDX},
    {Opcode::NOP,  AddressMode::ZPG_X},   {Opcode::CMP,  AddressMode::ZPG_X},   {Opcode::DEC,  AddressMode::ZPG_X}, {Opcode::DCP,  AddressMode::ZPG_X},
    {Opcode::CLD,  AddressMode::IMPL},    {Opcode::CMP,  AddressMode::ABS_Y},   {Opcode::NOP,  AddressMode::IMPL},  {Opcode::DCP,  AddressMode::ABS_Y},
    {Opcode::NOP,  AddressMode::ABS_X},   {Opcode::CMP,  AddressMode::ABS_X},   {Opcode::DEC,  AddressMode::ABS_X}, {Opcode::DCP,  AddressMode::ABS_X},

    // 0xE0-0xEF
    {Opcode::CPX,  AddressMode::IMT},     {Opcode::SBC,  AddressMode::IDX_IND}, {Opcode::NOP,  AddressMode::IMT},   {Opcode::ISB,  AddressMode::IDX_IND},
    {Opcode::CPX,  AddressMode::ZPG},     {Opcode::SBC,  AddressMode::ZPG},     {Opcode::INC,  AddressMode::ZPG},   {Opcode::ISB,  AddressMode::ZPG},
    {Opcode::INX,  AddressMode::IMPL},    {Opcode::SBC,  AddressMode::IMT},     {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::SBC,  AddressMode::IMT},
    {Opcode::CPX,  AddressMode::ABS},     {Opcode::SBC,  AddressMode::ABS},     {Opcode::INC,  AddressMode::ABS},   {Opcode::ISB,  AddressMode::ABS},

    // 0xF0-0xFF
    {Opcode::BEQ,  AddressMode::REL},     {Opcode::SBC,  AddressMode::IND_IDX}, {Opcode::NIL_OP, AddressMode::NONE}, {Opcode::ISB,  AddressMode::IND_IDX},
    {Opcode::NOP,  AddressMode::ZPG_X},   {Opcode::SBC,  AddressMode::ZPG_X},   {Opcode::INC,  AddressMode::ZPG_X}, {Opcode::ISB,  AddressMode::ZPG_X},
    {Opcode::SED,  AddressMode::IMPL},    {Opcode::SBC,  AddressMode::ABS_Y},   {Opcode::NOP,  AddressMode::IMPL},  {Opcode::ISB,  AddressMode::ABS_Y},
    {Opcode::NOP,  AddressMode::ABS_X},   {Opcode::SBC,  AddressMode::ABS_X},   {Opcode::INC,  AddressMode::ABS_X}, {Opcode::ISB,  AddressMode::ABS_X}
};

/* ============================================================================
 * Cycle Count Tables by Opcode
 * 
 * Base cycle counts for each opcode. Some opcodes have different cycles
 * depending on addressing mode or branch conditions.
 * ========================================================================== */

static constexpr uint8_t get_base_cycles(Opcode op, AddressMode mode) {
    // Special cases by opcode
    switch (op) {
        case Opcode::BRK:  return 7;
        case Opcode::PHP:  return 3;
        case Opcode::PLP:  return 4;
        case Opcode::PHA:  return 3;
        case Opcode::PLA:  return 4;
        case Opcode::RTI:  return 6;
        case Opcode::RTS:  return 6;
        case Opcode::JSR:  return 6;
        case Opcode::JMP:  return (mode == AddressMode::IND) ? 5 : 3;
        case Opcode::DEY:  return 2;
        case Opcode::TAY:  return 2;
        case Opcode::INY:  return 2;
        case Opcode::INX:  return 2;
        case Opcode::NOP:  return 2;  // Most NOPs are 2 cycles
        case Opcode::DEX:  return 2;
        case Opcode::CLC:  return 2;
        case Opcode::TYA:  return 2;
        case Opcode::TXA:  return 2;
        case Opcode::TXS:  return 2;
        case Opcode::TAX:  return 2;
        case Opcode::TSX:  return 2;
        case Opcode::SEC:  return 2;
        case Opcode::SED:  return 2;
        case Opcode::SEI:  return 2;
        case Opcode::CLD:  return 2;
        case Opcode::CLI:  return 2;
        case Opcode::CLV:  return 2;
        case Opcode::ASL:  return (mode == AddressMode::ACC) ? 2 :
                             (mode == AddressMode::ZPG) ? 5 :
                             (mode == AddressMode::ZPG_X) ? 6 :
                             (mode == AddressMode::ABS) ? 6 :
                             (mode == AddressMode::ABS_X) ? 7 : 2;
        case Opcode::LSR:  return (mode == AddressMode::ACC) ? 2 :
                             (mode == AddressMode::ZPG) ? 5 :
                             (mode == AddressMode::ZPG_X) ? 6 :
                             (mode == AddressMode::ABS) ? 6 :
                             (mode == AddressMode::ABS_X) ? 7 : 2;
        case Opcode::ROL:  return (mode == AddressMode::ACC) ? 2 :
                             (mode == AddressMode::ZPG) ? 5 :
                             (mode == AddressMode::ZPG_X) ? 6 :
                             (mode == AddressMode::ABS) ? 6 :
                             (mode == AddressMode::ABS_X) ? 7 : 2;
        case Opcode::ROR:  return (mode == AddressMode::ACC) ? 2 :
                             (mode == AddressMode::ZPG) ? 5 :
                             (mode == AddressMode::ZPG_X) ? 6 :
                             (mode == AddressMode::ABS) ? 6 :
                             (mode == AddressMode::ABS_X) ? 7 : 2;

        // Branch instructions - base is 2, +1 if taken, +1 if page crossed
        case Opcode::BCC:
        case Opcode::BCS:
        case Opcode::BEQ:
        case Opcode::BMI:
        case Opcode::BNE:
        case Opcode::BPL:
        case Opcode::BVC:
        case Opcode::BVS:
            return 2;

        // Load/Store instructions
        case Opcode::LDA:
        case Opcode::LDX:
        case Opcode::LDY:
            switch (mode) {
                case AddressMode::IMT:    return 2;
                case AddressMode::ZPG:    return 3;
                case AddressMode::ZPG_X:
                case AddressMode::ZPG_Y:  return 4;
                case AddressMode::ABS:    return 4;
                case AddressMode::ABS_X:
                case AddressMode::ABS_Y:  return 4;
                case AddressMode::IDX_IND: return 6;
                case AddressMode::IND_IDX: return 5;
                default: return 2;
            }

        case Opcode::STA:
        case Opcode::STX:
        case Opcode::STY:
            switch (mode) {
                case AddressMode::ZPG:    return 3;
                case AddressMode::ZPG_X:
                case AddressMode::ZPG_Y:  return 4;
                case AddressMode::ABS:    return 4;
                case AddressMode::ABS_X:
                case AddressMode::ABS_Y:  return 5;
                case AddressMode::IDX_IND: return 6;
                case AddressMode::IND_IDX: return 6;
                default: return 2;
            }

        // Compare instructions
        case Opcode::CMP:
        case Opcode::CPX:
        case Opcode::CPY:
            switch (mode) {
                case AddressMode::IMT:    return 2;
                case AddressMode::ZPG:    return 3;
                case AddressMode::ZPG_X:
                case AddressMode::ZPG_Y:  return 4;
                case AddressMode::ABS:    return 4;
                case AddressMode::ABS_X:
                case AddressMode::ABS_Y:  return 4;
                case AddressMode::IDX_IND: return 6;
                case AddressMode::IND_IDX: return 5;
                default: return 2;
            }

        // Arithmetic instructions
        case Opcode::ADC:
        case Opcode::SBC:
        case Opcode::AND:
        case Opcode::EOR:
        case Opcode::ORA:
            switch (mode) {
                case AddressMode::IMT:    return 2;
                case AddressMode::ZPG:    return 3;
                case AddressMode::ZPG_X:
                case AddressMode::ZPG_Y:  return 4;
                case AddressMode::ABS:    return 4;
                case AddressMode::ABS_X:
                case AddressMode::ABS_Y:  return 4;
                case AddressMode::IDX_IND: return 6;
                case AddressMode::IND_IDX: return 5;
                default: return 2;
            }

        // Increment/Decrement
        case Opcode::INC:
        case Opcode::DEC:
            switch (mode) {
                case AddressMode::ZPG:    return 5;
                case AddressMode::ZPG_X:  return 6;
                case AddressMode::ABS:    return 6;
                case AddressMode::ABS_X:  return 7;
                default: return 5;
            }

        // BIT instruction
        case Opcode::BIT:
            return (mode == AddressMode::ZPG) ? 3 : 4;

        // Unofficial opcodes - approximate cycles
        case Opcode::SLO:
        case Opcode::RLA:
        case Opcode::SRE:
        case Opcode::RRA:
        case Opcode::DCP:
        case Opcode::ISB:
        case Opcode::LAX:
        case Opcode::SAX:
            switch (mode) {
                case AddressMode::ZPG:    return 5;
                case AddressMode::ZPG_X:
                case AddressMode::ZPG_Y:  return 6;
                case AddressMode::ABS:    return 6;
                case AddressMode::ABS_X:
                case AddressMode::ABS_Y:  return 7;
                case AddressMode::IDX_IND: return 8;
                case AddressMode::IND_IDX: return 8;
                default: return 5;
            }

        case Opcode::ANC:
        case Opcode::ALR:
        case Opcode::ARR:
        case Opcode::ANE:
        case Opcode::AXS:
            return 2;

        case Opcode::LAS:
        case Opcode::SHA:
        case Opcode::SHS:
        case Opcode::SHX:
        case Opcode::SHY:
            return 5;

        // SKB/IGN are essentially NOPs with different cycle counts
        case Opcode::SKB:
        case Opcode::IGN:
            return 2;

        default:
            return 2;  // Default fallback
    }
}

/* ============================================================================
 * Flag Effect Tables
 * ========================================================================== */

static void set_flag_effects(Instruction6502& instr, Opcode op) {
    instr.flag_effects.affects_n = false;
    instr.flag_effects.affects_z = false;
    instr.flag_effects.affects_c = false;
    instr.flag_effects.affects_v = false;
    instr.flag_effects.affects_d = false;
    instr.flag_effects.affects_i = false;

    switch (op) {
        case Opcode::ADC:
        case Opcode::AND:
        case Opcode::ASL:
        case Opcode::CMP:
        case Opcode::CPX:
        case Opcode::CPY:
        case Opcode::DEC:
        case Opcode::EOR:
        case Opcode::INC:
        case Opcode::LSR:
        case Opcode::ORA:
        case Opcode::ROL:
        case Opcode::ROR:
        case Opcode::SBC:
            instr.flag_effects.affects_n = true;
            instr.flag_effects.affects_z = true;
            break;

        default:
            break;
    }

    // Carry flag
    switch (op) {
        case Opcode::ADC:
        case Opcode::ASL:
        case Opcode::ROL:
        case Opcode::SBC:
        case Opcode::CLC:
        case Opcode::SEC:
            instr.flag_effects.affects_c = true;
            break;

        default:
            break;
    }

    // Overflow flag
    switch (op) {
        case Opcode::ADC:
        case Opcode::BIT:
        case Opcode::CMP:
        case Opcode::ROL:
        case Opcode::SBC:
        case Opcode::CLV:
            instr.flag_effects.affects_v = true;
            break;

        default:
            break;
    }

    // Decimal flag
    switch (op) {
        case Opcode::CLD:
        case Opcode::SED:
            instr.flag_effects.affects_d = true;
            break;

        default:
            break;
    }

    // Interrupt flag
    switch (op) {
        case Opcode::CLI:
        case Opcode::SEI:
            instr.flag_effects.affects_i = true;
            break;

        default:
            break;
    }

    // N/Z flags for load/transfer instructions
    switch (op) {
        case Opcode::LDA:
        case Opcode::LDX:
        case Opcode::LDY:
        case Opcode::PLA:
        case Opcode::PLP:
        case Opcode::TAX:
        case Opcode::TAY:
        case Opcode::TSX:
        case Opcode::TXA:
        case Opcode::TYA:
            instr.flag_effects.affects_n = true;
            instr.flag_effects.affects_z = true;
            break;

        default:
            break;
    }
}

/* ============================================================================
 * Decoder Implementation
 * ========================================================================== */

Decoder6502::Decoder6502(const ROM& rom) : rom_(rom) {}

 Instruction6502 Decoder6502::decode(uint32_t full_addr) const {
    uint8_t bank = static_cast<uint8_t>(full_addr >> 16);
    uint16_t addr = static_cast<uint16_t>(full_addr & 0xFFFF);
    return decode(addr, bank);
}

 Instruction6502 Decoder6502::decode(uint16_t addr, uint8_t bank) const {
    Instruction6502 instr = {};
    instr.address = addr;
    instr.bank = bank;
    instr.length = 1;
    instr.cycles = 2;  // Default
    instr.cycles_branch = 0;
    instr.is_jump = false;
    instr.is_call = false;
    instr.is_return = false;
    instr.is_conditional = false;
    instr.is_terminator = false;
    instr.reads_memory = false;
    instr.writes_memory = false;

    // Read opcode
    uint8_t opcode = rom_.read_banked(bank, addr);
    instr.opcode = opcode;

    // Look up instruction in table
    const OpcodeEntry& entry = INSTRUCTION_LOOKUP[opcode];
    instr.opcode_type = entry.op;
    instr.mode = entry.mode;

    // Handle undefined/nil opcodes
    if (instr.opcode_type == Opcode::NIL_OP || instr.opcode_type == Opcode::UNDEFINED) {
        instr.length = 1;
        instr.cycles = 2;
        return instr;
    }

    // Set instruction length based on addressing mode
    switch (instr.mode) {
        case AddressMode::IMPL:
        case AddressMode::ACC:
            instr.length = 1;
            break;
        case AddressMode::REL:
        case AddressMode::IMT:
        case AddressMode::ZPG:
        case AddressMode::ZPG_X:
        case AddressMode::ZPG_Y:
        case AddressMode::IDX_IND:
            instr.length = 2;
            break;
        case AddressMode::ABS:
        case AddressMode::ABS_X:
        case AddressMode::ABS_Y:
        case AddressMode::IND:
        case AddressMode::IND_IDX:
            instr.length = 3;
            break;
        default:
            instr.length = 1;
            break;
    }

    // Read operands based on addressing mode
    switch (instr.mode) {
        case AddressMode::IMT:
        case AddressMode::ZPG:
        case AddressMode::ZPG_X:
        case AddressMode::ZPG_Y:
        case AddressMode::IDX_IND:
            instr.imm8 = rom_.read_banked(bank, addr + 1);
            break;

        case AddressMode::ABS:
        case AddressMode::ABS_X:
        case AddressMode::ABS_Y:
        case AddressMode::IND:
        case AddressMode::IND_IDX:
            instr.imm16 = read_u16(addr + 1, bank);
            break;

        case AddressMode::REL:
            instr.imm8 = rom_.read_banked(bank, addr + 1);
            instr.offset = static_cast<int8_t>(instr.imm8);
            break;

        default:
            break;
    }

    // Set cycle count
    instr.cycles = get_base_cycles(instr.opcode_type, instr.mode);

    // Set flag effects
    set_flag_effects(instr, instr.opcode_type);

    // Set control flow flags
    switch (instr.opcode_type) {
        case Opcode::JMP:
            instr.is_jump = true;
            instr.is_terminator = true;
            break;

        case Opcode::BCC:
        case Opcode::BCS:
        case Opcode::BEQ:
        case Opcode::BMI:
        case Opcode::BNE:
        case Opcode::BPL:
        case Opcode::BVC:
        case Opcode::BVS:
            instr.is_jump = true;
            instr.is_conditional = true;
            instr.is_terminator = true;
            instr.cycles_branch = instr.cycles + 1;  // +1 if taken, +1 more if page crossed
            break;

        case Opcode::JSR:
            instr.is_call = true;
            instr.is_terminator = true;
            break;

        case Opcode::RTS:
        case Opcode::RTI:
            instr.is_return = true;
            instr.is_terminator = true;
            break;

        case Opcode::BRK:
            instr.is_terminator = true;
            break;

        default:
            break;
    }

    // Set memory access flags
    switch (instr.opcode_type) {
        case Opcode::LDA:
        case Opcode::LDX:
        case Opcode::LDY:
        case Opcode::AND:
        case Opcode::ORA:
        case Opcode::EOR:
        case Opcode::ADC:
        case Opcode::SBC:
        case Opcode::CMP:
        case Opcode::BIT:
        case Opcode::CPX:
        case Opcode::CPY:
        case Opcode::LAX:
            instr.reads_memory = true;
            break;

        case Opcode::STA:
        case Opcode::STX:
        case Opcode::STY:
        case Opcode::SAX:
        case Opcode::SHA:
        case Opcode::SHS:
        case Opcode::SHX:
        case Opcode::SHY:
            instr.writes_memory = true;
            break;

        case Opcode::INC:
        case Opcode::DEC:
        case Opcode::ASL:
        case Opcode::LSR:
        case Opcode::ROL:
        case Opcode::ROR:
        case Opcode::SLO:
        case Opcode::SRE:
        case Opcode::RLA:
        case Opcode::RRA:
        case Opcode::DCP:
        case Opcode::ISB:
            instr.reads_memory = true;
            instr.writes_memory = true;
            break;

        default:
            break;
    }

    return instr;
}

uint16_t Decoder6502::read_u16(uint16_t addr, uint8_t bank) const {
    uint8_t lo = rom_.read_banked(bank, addr);
    uint8_t hi = rom_.read_banked(bank, addr + 1);
    return static_cast<uint16_t>(lo | (hi << 8));
}

/* ============================================================================
 * Decoder Interface Functions
 * ========================================================================== */

std::vector<Instruction6502> decode_bank_6502(const ROM& rom, uint8_t bank) {
    std::vector<Instruction6502> instructions;
    Decoder6502 decoder(rom);

    // Get ROM bank size - for NES, PRG ROM is typically 16KB or 32KB
    // Use the PRG size from header, defaulting to full ROM size
    size_t bank_size = rom.prg_size() > 0 ? rom.prg_size() : rom.size();
    if (bank_size == 0) {
        bank_size = rom.size();
    }

    // Calculate offset for the specified bank
    // NES PRG ROM banks are typically 16KB each
    constexpr size_t PRG_BANK_SIZE = 0x4000;  // 16KB
    size_t offset = bank * PRG_BANK_SIZE;
    size_t end = offset + bank_size;

    // Clamp to actual ROM size
    if (end > rom.size()) {
        end = rom.size();
    }
    if (offset >= rom.size()) {
        return instructions;  // Empty result if offset is beyond ROM
    }

    // Linear sweep through the bank
    for (size_t i = offset; i < end; ) {
        uint16_t addr = static_cast<uint16_t>(i % 0x10000);
        Instruction6502 instr = decoder.decode(addr, bank);

        if (instr.opcode_type == Opcode::NIL_OP ||
            instr.opcode_type == Opcode::UNDEFINED) {
            // Skip undefined opcodes but still record them
            instructions.push_back(instr);
            i += instr.length;
            continue;
        }

        instructions.push_back(instr);
        i += instr.length;

        // Safety limit
        if (instructions.size() > 100000) {
            break;
        }
    }

    return instructions;
}

uint8_t get_cycle_count_6502(const Instruction6502& instr, bool branch_taken) {
    uint8_t cycles = instr.cycles;
    if (branch_taken && instr.is_conditional) {
        cycles = instr.cycles_branch > 0 ? instr.cycles_branch : cycles + 1;
    }
    return cycles;
}

/* ============================================================================
 * Disassembly Functions
 * ========================================================================== */

const char* opcode_name(Opcode op) {
    return OPCODE_NAMES[static_cast<size_t>(op)];
}

const char* address_mode_name(AddressMode mode) {
    return MODE_NAMES[static_cast<size_t>(mode)];
}

const char* reg8_name_6502(Reg8_6502 reg) {
    return REG8_NAMES[static_cast<size_t>(reg)];
}

std::string Instruction6502::bytes_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(opcode);
    if (length > 1) {
        // For 2-byte instructions, show imm8
        // For 3-byte instructions, show low byte of imm16
        uint8_t byte2 = (length > 2) ? (imm16 & 0xFF) : imm8;
        oss << " " << std::setw(2) << std::setfill('0') << static_cast<int>(byte2);
    }
    if (length > 2) {
        oss << " " << std::setw(2) << std::setfill('0') << static_cast<int>((imm16 >> 8) & 0xFF);
    }
    return oss.str();
}

std::string Instruction6502::disassemble() const {
    std::ostringstream oss;

    // Address
    oss << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(address) << ": ";

    // Bytes
    oss << bytes_hex();
    
    // Calculate and add padding
    std::string bytes_str = oss.str();
    size_t padding = (bytes_str.length() < 19) ? (19 - bytes_str.length()) : 1;
    oss << std::setfill(' ') << std::string(padding, ' ');

    // Mnemonic
    const char* op_name = opcode_name(opcode_type);
    oss << std::setw(4) << std::left << op_name;

    // Operands based on addressing mode
    switch (mode) {
        case AddressMode::IMPL:
        case AddressMode::ACC:
            // No operand for implied/accumulator mode
            break;

        case AddressMode::IMT:
            oss << " #$" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(imm8);
            break;

        case AddressMode::ZPG:
            oss << " $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(imm8);
            break;

        case AddressMode::ZPG_X:
            oss << " $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(imm8) << ",X";
            break;

        case AddressMode::ZPG_Y:
            oss << " $" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(imm8) << ",Y";
            break;

        case AddressMode::ABS:
            oss << " $" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(imm16);
            break;

        case AddressMode::ABS_X:
            oss << " $" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(imm16) << ",X";
            break;

        case AddressMode::ABS_Y:
            oss << " $" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(imm16) << ",Y";
            break;

        case AddressMode::REL: {
            uint16_t target = static_cast<uint16_t>(address + length + offset);
            oss << " $" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(target);
            break;
        }

        case AddressMode::IND:
            oss << " ($" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(imm16) << ")";
            break;

        case AddressMode::IDX_IND:
            oss << " ($" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(imm8) << ",X)";
            break;

        case AddressMode::IND_IDX:
            oss << " ($" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(imm8) << "),Y";
            break;

        default:
            break;
    }

    return oss.str();
}

std::string disassemble_6502(const Instruction6502& instr) {
    return instr.disassemble();
}

} // namespace nesrecomp
