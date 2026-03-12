#!/usr/bin/env python3
"""
build_test01.py - Build test01_simple.nes from raw 6502 opcodes
Creates a minimal iNES ROM with basic 6502 instructions for testing the recompiler.
"""

import struct
import sys
import os

# 6502 Opcodes (reference: https://www.masswerk.at/6502/6502_instruction_set.html)
# Format: (opcode, addressing_mode, cycles, description)

def build_prg_rom():
    """Build PRG ROM with 6502 instructions starting at $8000"""
    prg = bytearray()

    # Reset vector entry point at $8000
    # LDX #$FF - Initialize stack pointer
    prg += bytes([0xA2, 0xFF])  # LDX #imm

    # TXS - Transfer X to Stack Pointer
    prg += bytes([0x9A])

    # LDA #$00
    prg += bytes([0xA9, 0x00])

    # STA $0000 - Store to zero page
    prg += bytes([0x85, 0x00])

    # LDA #$12
    prg += bytes([0xA9, 0x12])
    # STA $0001
    prg += bytes([0x85, 0x01])

    # LDA #$34
    prg += bytes([0xA9, 0x34])
    # STA $0002
    prg += bytes([0x85, 0x02])

    # LDX #$05
    prg += bytes([0xA2, 0x05])
    # STX $0003
    prg += bytes([0x86, 0x03])

    # LDY #$0A
    prg += bytes([0xA0, 0x0A])
    # STY $0004
    prg += bytes([0x84, 0x04])

    # Test ADC (Add with Carry)
    # LDA #$10
    prg += bytes([0xA9, 0x10])
    # CLC - Clear Carry
    prg += bytes([0x18])
    # ADC #$05
    prg += bytes([0x69, 0x05])
    # STA $0005
    prg += bytes([0x85, 0x05])

    # Test SBC (Subtract with Carry)
    # LDA #$20
    prg += bytes([0xA9, 0x20])
    # SEC - Set Carry
    prg += bytes([0x38])
    # SBC #$08
    prg += bytes([0xE9, 0x08])
    # STA $0006
    prg += bytes([0x85, 0x06])

    # Test AND
    # LDA #$FF
    prg += bytes([0xA9, 0xFF])
    # AND #$0F
    prg += bytes([0x29, 0x0F])
    # STA $0007
    prg += bytes([0x85, 0x07])

    # Test ORA
    # LDA #$F0
    prg += bytes([0xA9, 0xF0])
    # ORA #$0F
    prg += bytes([0x09, 0x0F])
    # STA $0008
    prg += bytes([0x85, 0x08])

    # Test EOR
    # LDA #$AA
    prg += bytes([0xA9, 0xAA])
    # EOR #$55
    prg += bytes([0x49, 0x55])
    # STA $0009
    prg += bytes([0x85, 0x09])

    # Test branches - BEQ
    # LDA #$00
    prg += bytes([0xA9, 0x00])
    # BEQ branch_taken (offset calculated below)
    beq_offset = 0x03  # Skip next LDA instruction (2 bytes) + NOP for alignment
    prg += bytes([0xF0, beq_offset])
    # LDA #$FF (should not execute)
    prg += bytes([0xA9, 0xFF])
    # branch_taken: STA $000A
    prg += bytes([0x85, 0x0A])

    # Test BNE
    # LDA #$01
    prg += bytes([0xA9, 0x01])
    # BNE branch_taken2
    bne_offset = 0x03
    prg += bytes([0xD0, bne_offset])
    # LDA #$FF (should not execute)
    prg += bytes([0xA9, 0xFF])
    # branch_taken2: STA $000B
    prg += bytes([0x85, 0x0B])

    # Test BMI (Branch if Minus/Negative)
    # LDA #$80 (negative value, bit 7 set)
    prg += bytes([0xA9, 0x80])
    # BMI branch_neg
    bmi_offset = 0x03
    prg += bytes([0x30, bmi_offset])
    # LDA #$FF (should not execute)
    prg += bytes([0xA9, 0xFF])
    # branch_neg: STA $000C
    prg += bytes([0x85, 0x0C])

    # Test BPL (Branch if Plus/Positive)
    # LDA #$7F (positive value)
    prg += bytes([0xA9, 0x7F])
    # BPL branch_pos
    bpl_offset = 0x03
    prg += bytes([0x10, bpl_offset])
    # LDA #$FF (should not execute)
    prg += bytes([0xA9, 0xFF])
    # branch_pos: STA $000D
    prg += bytes([0x85, 0x0D])

    # Test JSR/RTS
    # Calculate offset to subroutine
    current_addr = 0x8000 + len(prg)
    subroutine_addr = current_addr + 7 + 3  # After JSR + STA, accounting for JSR (3 bytes) + STA (3 bytes)

    # JSR subroutine
    prg += bytes([0x20])
    prg += struct.pack('<H', subroutine_addr)  # Little-endian address

    # STA $000E (store result from subroutine)
    prg += bytes([0x85, 0x0E])

    # JMP infinite_loop
    jmp_addr = 0x8000 + len(prg) + 3  # After JMP instruction
    prg += bytes([0x4C])
    prg += struct.pack('<H', jmp_addr)

    # Subroutine at current position
    # LDA #$42
    prg += bytes([0xA9, 0x42])
    # RTS
    prg += bytes([0x60])

    # Infinite loop
    # JMP to self
    loop_addr = 0x8000 + len(prg)
    prg += bytes([0x4C])
    prg += struct.pack('<H', loop_addr)

    # Pad to minimum size (need at least vectors at end)
    # For a 16KB PRG ROM, we need to reach $C000 - 6 bytes for vectors
    min_prg_size = 0x4000 - 6  # 16KB - 6 bytes for vectors
    while len(prg) < min_prg_size:
        prg += bytes([0xEA])  # NOP for padding

    # Add interrupt vectors at end of PRG ROM
    # These go at $FFFA, $FFFC, $FFFE in the ROM address space
    # For 2KB PRG at $8000-$87FF, vectors are at $87FA, $87FC, $87FE
    nmi_handler = 0x8000 + len(prg)  # Point to a safe RTI handler
    reset_vector = 0x8000  # Reset points to start
    irq_handler = 0x8000 + len(prg)

    # NMI vector
    prg += struct.pack('<H', nmi_handler)
    # Reset vector
    prg += struct.pack('<H', reset_vector)
    # IRQ vector
    prg += struct.pack('<H', irq_handler)

    return bytes(prg)


def build_ines_header(prg_size_kb=2, chr_size_kb=0, mapper=0):
    """
    Build iNES header (16 bytes)
    Reference: https://www.nesdev.org/wiki/INES
    """
    header = bytearray()

    # Bytes 0-3: Magic number "NES\x1A"
    header += b'NES\x1A'

    # Byte 4: PRG ROM size in 16KB units
    header += bytes([prg_size_kb // 16])

    # Byte 5: CHR ROM size in 8KB units
    header += bytes([chr_size_kb // 8])

    # Byte 6: Flags 6
    # Bit 0: Vertical mirroring (0) or Horizontal (1)
    # Bit 1: Battery-backed RAM
    # Bit 2: Trainer
    # Bit 3-5: Mapper low bits
    # Bit 6: NES 2.0 flag (0 = NES 1.0)
    # Bit 7: Four-screen VRAM
    flags6 = 0x00  # Vertical mirroring, no battery, mapper 0
    header += bytes([flags6])

    # Byte 7: Flags 7
    # Bit 0-3: Mapper high bits (for NES 1.0, usually 0)
    # Bit 4: VS Unisystem
    # Bit 5: PlayChoice-10
    # Bit 6-7: Console type
    flags7 = 0x00
    header += bytes([flags7])

    # Bytes 8-15: Padding (zeros for NES 1.0)
    header += bytes([0x00] * 8)

    return bytes(header)


def build_rom(output_path):
    """Build complete .nes ROM file"""
    # Build header (16KB PRG ROM)
    header = build_ines_header(prg_size_kb=16)

    # Build PRG ROM
    prg = build_prg_rom()

    # Write to file
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(prg)

    return len(header) + len(prg)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, 'test01_simple.nes')

    print(f"Building test01_simple.nes...")
    size = build_rom(output_path)
    print(f"Output: {output_path}")
    print(f"Size: {size} bytes ({size // 1024} KB)")

    # Verify with hexdump
    print("\nFirst 64 bytes (hex):")
    with open(output_path, 'rb') as f:
        data = f.read(64)
        for i in range(0, len(data), 16):
            hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
            ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
            print(f"{0x8000 + i:04x}: {hex_str:<48} {ascii_str}")

    print("\nROM built successfully!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
