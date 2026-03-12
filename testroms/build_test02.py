#!/usr/bin/env python3
"""
build_test02.py - Build test02_transfers.nes from raw 6502 opcodes
Tests transfer, stack, increment/decrement, shift/rotate, and compare instructions.
"""

import struct
import sys
import os


def build_prg_rom():
    """Build PRG ROM with 6502 instructions"""
    prg = bytearray()

    # Reset entry at $8000
    # LDX #$FF, TXS - Initialize stack
    prg += bytes([0xA2, 0xFF, 0x9A])

    # === Transfer Instructions ===
    # LDA #$11
    prg += bytes([0xA9, 0x11])
    # TAX - Transfer A to X
    prg += bytes([0xAA])
    # TAY - Transfer A to Y
    prg += bytes([0xA8])
    # TXA - Transfer X to A
    prg += bytes([0x8A])
    # TYA - Transfer Y to A
    prg += bytes([0x98])

    # === Stack Operations ===
    # LDA #$AA
    prg += bytes([0xA9, 0xAA])
    # PHA - Push A
    prg += bytes([0x48])
    # LDA #$BB
    prg += bytes([0xA9, 0xBB])
    # PHA
    prg += bytes([0x48])
    # PLA - Pull A
    prg += bytes([0x68])
    # PLA
    prg += bytes([0x68])

    # === Increment/Decrement ===
    # LDA #$00, STA $0000
    prg += bytes([0xA9, 0x00, 0x85, 0x00])
    # INC $0000 - Increment memory
    prg += bytes([0xE6, 0x00])
    # DEC $0000 - Decrement memory
    prg += bytes([0xC6, 0x00])
    # INX - Increment X
    prg += bytes([0xE8])
    # INY - Increment Y
    prg += bytes([0xC8])
    # DEX - Decrement X
    prg += bytes([0xCA])
    # DEY - Decrement Y
    prg += bytes([0x88])

    # === Shift and Rotate Instructions ===
    # LDA #$01
    prg += bytes([0xA9, 0x01])
    # ASL A - Arithmetic Shift Left (accumulator)
    prg += bytes([0x0A])
    # ASL $0001 - Shift memory
    prg += bytes([0x06, 0x01])
    # LSR A - Logical Shift Right
    prg += bytes([0x4A])
    # LSR $0002
    prg += bytes([0x46, 0x02])
    # ROL A - Rotate Left
    prg += bytes([0x2A])
    # ROL $0003
    prg += bytes([0x26, 0x03])
    # ROR A - Rotate Right
    prg += bytes([0x6A])
    # ROR $0004
    prg += bytes([0x66, 0x04])

    # === Compare Instructions ===
    # LDA #$50
    prg += bytes([0xA9, 0x50])
    # CMP #$50 - Equal (Z=1)
    prg += bytes([0xC9, 0x50])
    # CMP #$40 - A > imm (C=1, N=0)
    prg += bytes([0xC9, 0x40])
    # CMP #$60 - A < imm (C=0, N=1)
    prg += bytes([0xC9, 0x60])

    # LDX #$30
    prg += bytes([0xA2, 0x30])
    # CPX #$30 - Equal
    prg += bytes([0xE0, 0x30])
    # CPX #$20 - X > imm
    prg += bytes([0xE0, 0x20])
    # CPX #$40 - X < imm
    prg += bytes([0xE0, 0x40])

    # LDY #$20
    prg += bytes([0xA0, 0x20])
    # CPY #$20 - Equal
    prg += bytes([0xC0, 0x20])
    # CPY #$10 - Y > imm
    prg += bytes([0xC0, 0x10])
    # CPY #$30 - Y < imm
    prg += bytes([0xC0, 0x30])

    # === BIT Instruction ===
    # LDA #$FF
    prg += bytes([0xA9, 0xFF])
    # BIT $0005 - Test bits in memory
    prg += bytes([0x2C, 0x05, 0x00])

    # === Flag Operations ===
    # CLC - Clear Carry
    prg += bytes([0x18])
    # SEC - Set Carry
    prg += bytes([0x38])
    # CLI - Clear Interrupt Disable
    prg += bytes([0x58])
    # SEI - Set Interrupt Disable
    prg += bytes([0x78])
    # CLV - Clear Overflow
    prg += bytes([0xB8])

    # === Branch on Flags ===
    # BCC - Branch if Carry Clear
    prg += bytes([0x90, 0x02])  # Skip next NOP
    prg += bytes([0xEA])        # NOP

    # BCS - Branch if Carry Set
    prg += bytes([0xB0, 0x05])  # Skip SEC + branch target
    prg += bytes([0x38])        # SEC
    # Branch target here

    # BVC - Branch if Overflow Clear
    prg += bytes([0x50, 0x02])
    prg += bytes([0xEA])

    # BVS - Branch if Overflow Set (V is clear, won't branch)
    prg += bytes([0x70, 0x02])
    prg += bytes([0xEA])

    # === Complex Branching ===
    # LDA #$05, CMP #$05, BEQ
    prg += bytes([0xA9, 0x05, 0xC9, 0x05, 0xF0, 0x03])
    prg += bytes([0xEA])  # Skip this if equal
    # Equal branch target
    # LDA #$10, STA $0010
    prg += bytes([0xA9, 0x10, 0x85, 0x10])

    # === Subroutine Call ===
    # Calculate subroutine address
    subroutine_addr = 0x8000 + len(prg) + 7  # After JSR + JSR + JMP

    # LDA #$42, JSR StoreValue
    prg += bytes([0xA9, 0x42])
    prg += bytes([0x20])
    prg += struct.pack('<H', subroutine_addr)

    # LDA #$FF, JSR StoreValue
    prg += bytes([0xA9, 0xFF])
    prg += bytes([0x20])
    prg += struct.pack('<H', subroutine_addr)

    # JMP InfiniteLoop
    loop_addr = 0x8000 + len(prg) + 3 + 6 + 3  # After JMP + subroutine + JMP
    prg += bytes([0x4C])
    prg += struct.pack('<H', loop_addr)

    # Subroutine: StoreValue
    # STA $0020, RTS
    prg += bytes([0x85, 0x20, 0x60])

    # Infinite Loop
    final_loop = 0x8000 + len(prg)
    prg += bytes([0x4C])
    prg += struct.pack('<H', final_loop)

    # Pad to minimum size
    min_prg_size = 0x4000 - 6  # 16KB - 6 bytes for vectors
    while len(prg) < min_prg_size:
        prg += bytes([0xEA])

    # Interrupt vectors
    nmi_handler = 0x8000 + len(prg)
    reset_vector = 0x8000
    irq_handler = 0x8000 + len(prg)

    prg += struct.pack('<H', nmi_handler)  # NMI
    prg += struct.pack('<H', reset_vector)  # Reset
    prg += struct.pack('<H', irq_handler)  # IRQ

    return bytes(prg)


def build_ines_header(prg_size_kb=2, chr_size_kb=0):
    """Build iNES header (16 bytes)"""
    header = bytearray()
    header += b'NES\x1A'
    header += bytes([prg_size_kb // 16])
    header += bytes([chr_size_kb // 8])
    header += bytes([0x00])  # Flags 6: vertical mirroring, mapper 0
    header += bytes([0x00])  # Flags 7
    header += bytes([0x00] * 8)  # Padding
    return bytes(header)


def build_rom(output_path):
    """Build complete .nes ROM file"""
    header = build_ines_header(prg_size_kb=16)
    prg = build_prg_rom()

    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(prg)

    return len(header) + len(prg)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, 'test02_transfers.nes')

    print(f"Building test02_transfers.nes...")
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
            print(f"{i:04x}: {hex_str:<48} {ascii_str}")

    print("\nROM built successfully!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
