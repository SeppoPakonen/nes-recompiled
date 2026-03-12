#!/usr/bin/env python3
"""
build_test03.py - Build test03_addressing.nes from raw 6502 opcodes
Tests various 6502 addressing modes: zero page, indexed, indirect, absolute.
"""

import struct
import sys
import os


def build_prg_rom():
    """Build PRG ROM with 6502 instructions testing addressing modes"""
    prg = bytearray()

    # Reset entry at $8000
    # LDX #$FF, TXS, LDY #$00 - Initialize
    prg += bytes([0xA2, 0xFF, 0x9A, 0xA0, 0x00])

    # === Zero Page Addressing ===
    # LDA #$11, STA $10, LDA $10
    prg += bytes([0xA9, 0x11, 0x85, 0x10, 0xA5, 0x10])

    # === Zero Page,X Addressing ===
    # LDX #$05, LDA #$22, STA $20,X, LDA $20,X
    prg += bytes([0xA2, 0x05, 0xA9, 0x22, 0x95, 0x20, 0xB5, 0x20])

    # === Zero Page,Y Addressing ===
    # LDY #$03, LDA #$33, STA $30,Y, LDA $30,Y
    prg += bytes([0xA0, 0x03, 0xA9, 0x33, 0x91, 0x30, 0xB1, 0x30])
    # Wait, 0x91 is STA (indirect),Y - need 0x99 for STA zp,Y
    # Fix: STA zp,Y = 0x96, LDA zp,Y = 0xB6... no wait
    # Actually: STA zp,Y doesn't exist on 6502! Only STA zp,X
    # Let me fix this - use absolute,Y instead
    prg = prg[:-4]  # Remove last 4 bytes
    # LDY #$03, LDA #$33, STA $0033 (absolute), LDA $0033
    prg += bytes([0xA0, 0x03, 0xA9, 0x33, 0x8D, 0x33, 0x00, 0xAD, 0x33, 0x00])

    # === Absolute Addressing ===
    # LDA #$44, STA $0100, LDA $0100
    prg += bytes([0xA9, 0x44, 0x8D, 0x00, 0x01, 0xAD, 0x00, 0x01])

    # === Absolute,X Addressing ===
    # LDX #$10, LDA #$55, STA $0200,X, LDA $0200,X
    prg += bytes([0xA2, 0x10, 0xA9, 0x55, 0x9D, 0x00, 0x02, 0xBD, 0x00, 0x02])

    # === Absolute,Y Addressing ===
    # LDY #$20, LDA #$66, STA $0300,Y, LDA $0300,Y
    prg += bytes([0xA0, 0x20, 0xA9, 0x66, 0x99, 0x00, 0x03, 0xB9, 0x00, 0x03])

    # === Indirect,X Addressing (Indexed Indirect) ===
    # Set up pointer at $40 pointing to $0050
    # LDA #$50, STA $40, LDA #$00, STA $41
    prg += bytes([0xA9, 0x50, 0x85, 0x40, 0xA9, 0x00, 0x85, 0x41])
    # LDA #$77, STA $0050 (data location)
    prg += bytes([0xA9, 0x77, 0x8D, 0x50, 0x00])
    # LDX #$00, LDA ($40,X)
    prg += bytes([0xA2, 0x00, 0xA1, 0x40])

    # === Indirect,Y Addressing (Indirect Indexed) ===
    # Set up pointer at $60 pointing to $0070
    # LDA #$70, STA $60, LDA #$00, STA $61
    prg += bytes([0xA9, 0x70, 0x85, 0x60, 0xA9, 0x00, 0x85, 0x61])
    # LDY #$05, LDA #$88, STA $0075 (data at pointer+Y)
    prg += bytes([0xA0, 0x05, 0xA9, 0x88, 0x8D, 0x75, 0x00])
    # LDA ($60),Y
    prg += bytes([0xB1, 0x60])

    # === Immediate Addressing ===
    # LDA #$99, LDX #$AA, LDY #$BB
    prg += bytes([0xA9, 0x99, 0xA2, 0xAA, 0xA0, 0xBB])

    # === Relative Addressing (Branches) ===
    # LDA #$00, BEQ Branch1
    beq_target = 0x8000 + len(prg) + 4  # After BEQ + skip instruction
    prg += bytes([0xA9, 0x00, 0xF0, 0x03])  # BEQ +3 (skip next LDA)
    prg += bytes([0xA9, 0xFF])  # Skipped if branch taken
    # Branch1: STA $70
    prg += bytes([0x85, 0x70])

    # LDA #$01, BNE Branch2
    prg += bytes([0xA9, 0x01, 0xD0, 0x03])  # BNE +3
    prg += bytes([0xA9, 0xFF])
    # Branch2: STA $71
    prg += bytes([0x85, 0x71])

    # === Implied Addressing ===
    # CLC, SEC, NOP, CLV
    prg += bytes([0x18, 0x38, 0xEA, 0xB8])

    # === Accumulator Addressing ===
    # LDA #$01, ASL A, LSR A, ROL A, ROR A
    prg += bytes([0xA9, 0x01, 0x0A, 0x4A, 0x2A, 0x6A])

    # === Test Subroutine with Stack ===
    # JSR TestSub
    subroutine_addr = 0x8000 + len(prg) + 3 + 3  # After JSR + JMP
    prg += bytes([0x20])
    prg += struct.pack('<H', subroutine_addr)

    # JMP InfiniteLoop
    loop_addr = 0x8000 + len(prg) + 14 + 3  # After subroutine + JMP
    prg += bytes([0x4C])
    prg += struct.pack('<H', loop_addr)

    # TestSub: PHA, TXA, PHA, TYA, PHA, PLA, TAY, PLA, TAX, PLA, RTS
    prg += bytes([0x48, 0x8A, 0x48, 0x98, 0x48, 0x68, 0xA8, 0x68, 0xAA, 0x68, 0x60])

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

    prg += struct.pack('<H', nmi_handler)
    prg += struct.pack('<H', reset_vector)
    prg += struct.pack('<H', irq_handler)

    return bytes(prg)


def build_ines_header(prg_size_kb=2, chr_size_kb=0):
    """Build iNES header (16 bytes)"""
    header = bytearray()
    header += b'NES\x1A'
    header += bytes([prg_size_kb // 16])
    header += bytes([chr_size_kb // 8])
    header += bytes([0x00])
    header += bytes([0x00])
    header += bytes([0x00] * 8)
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
    output_path = os.path.join(script_dir, 'test03_addressing.nes')

    print(f"Building test03_addressing.nes...")
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
