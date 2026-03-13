#!/usr/bin/env python3
"""
build_test_mmc1.py - Build MMC1 test ROM
Creates a minimal iNES ROM with MMC1 mapper (mapper 1) for testing bank switching.
"""

import struct
import sys
import os

def build_prg_rom():
    """Build PRG ROM with 6502 instructions and MMC1 bank switching code"""
    prg = bytearray()

    # Reset vector entry point at $8000
    # Initialize stack pointer
    prg += bytes([0xA2, 0xFF])  # LDX #$FF
    prg += bytes([0x9A])        # TXS

    # Clear decimal mode, disable interrupts
    prg += bytes([0xD8])        # CLD
    prg += bytes([0x78])        # SEI

    # Initialize some RAM values
    prg += bytes([0xA9, 0x01])  # LDA #$01
    prg += bytes([0x85, 0x00])  # STA $0000

    # Test MMC1 bank switching
    # MMC1 uses serial shift register at $8000-$FFFF
    # Write 5 bits serially to set PRG bank

    # Reset MMC1 shift register (write 1 to bit 7)
    prg += bytes([0xA9, 0x80])  # LDA #$80
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000 (reset)

    # Write control register: 0b01100 = 0x0C
    # Bit 4: PRG RAM enable (1)
    # Bit 3: PRG ROM size mode (1 = 32KB mode, 0 = 16KB mode)
    # Bit 2-1: Mirroring (10 = vertical)
    # Bit 0: CHR bank mode (0 = 8KB mode)
    # Value: 0b01100 = write sequence: 0,0,1,1,0

    # Reset first
    prg += bytes([0xA9, 0x80])  # LDA #$80
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000

    # Write bit 0 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000

    # Write bit 1 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000

    # Write bit 2 (1)
    prg += bytes([0xA9, 0x01])  # LDA #$01
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000

    # Write bit 3 (1)
    prg += bytes([0xA9, 0x01])  # LDA #$01
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000

    # Write bit 4 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0x80])  # STA $8000

    # Now write PRG bank register at $E000
    # Reset first
    prg += bytes([0xA9, 0x80])  # LDA #$80
    prg += bytes([0x8D, 0x00, 0xE0])  # STA $E000

    # Write bank number 1 (binary 00001)
    # Bit 0 (1)
    prg += bytes([0xA9, 0x01])  # LDA #$01
    prg += bytes([0x8D, 0x00, 0xE0])  # STA $E000
    # Bit 1 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0xE0])  # STA $E000
    # Bit 2 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0xE0])  # STA $E000
    # Bit 3 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0xE0])  # STA $E000
    # Bit 4 (0)
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0x8D, 0x00, 0xE0])  # STA $E000

    # Store a value to verify bank switch worked
    prg += bytes([0xA9, 0x42])  # LDA #$42
    prg += bytes([0x8D, 0x00, 0x60])  # STA $6000 (PRG RAM)

    # Read it back
    prg += bytes([0xAD, 0x00, 0x60])  # LDA $6000
    prg += bytes([0x85, 0x01])  # STA $0001

    # Simple branch test
    prg += bytes([0xA9, 0x00])  # LDA #$00
    prg += bytes([0xF0, 0x03])  # BEQ +3
    prg += bytes([0xA9, 0xFF])  # LDA #$FF (skip)
    prg += bytes([0x85, 0x02])  # STA $0002

    # JSR/RTS test
    jsr_addr = 0x8000 + len(prg) + 3
    prg += bytes([0x20])
    prg += struct.pack('<H', jsr_addr)  # JSR subroutine
    prg += bytes([0x85, 0x03])  # STA $0003

    # JMP to infinite loop
    jmp_addr = 0x8000 + len(prg) + 3
    prg += bytes([0x4C])
    prg += struct.pack('<H', jmp_addr)  # JMP infinite_loop

    # Subroutine
    prg += bytes([0xA9, 0x55])  # LDA #$55
    prg += bytes([0x60])        # RTS

    # Infinite loop
    loop_addr = 0x8000 + len(prg)
    prg += bytes([0x4C])
    prg += struct.pack('<H', loop_addr)  # JMP infinite_loop

    # Pad to 32KB (MMC1 typically has 32KB PRG ROM = 2 banks of 16KB)
    # We need to reach 0x8000 bytes (32KB) total
    while len(prg) < 0x8000 - 6:  # -6 for vectors
        prg += bytes([0xEA])  # NOP padding

    # Interrupt vectors at end of PRG ROM
    nmi_handler = 0x8000 + len(prg)
    reset_vector = 0x8000
    irq_handler = 0x8000 + len(prg)

    prg += struct.pack('<H', nmi_handler)  # NMI vector
    prg += struct.pack('<H', reset_vector)  # Reset vector
    prg += struct.pack('<H', irq_handler)   # IRQ vector

    return bytes(prg)


def build_ines_header(prg_size_kb=32, chr_size_kb=0, mapper=1):
    """
    Build iNES header (16 bytes) for MMC1
    Reference: https://www.nesdev.org/wiki/INES
    
    Mapper number is calculated as: (mapper_high << 4) | mapper_low
    where mapper_low is bits 4-7 of byte 6, and mapper_high is bits 4-7 of byte 7.
    For mapper 1: mapper_low = 0001, mapper_high = 0000
    So byte 6 = (mirroring) | (mapper_low << 4) = 0x00 | 0x10 = 0x10
    And byte 7 = 0x00
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
    # Bit 3: Four-screen VRAM (or NES 2.0 flag)
    # Bit 4-7: Mapper low nibble (bits 0-3)
    # For mapper 1: low nibble = 0001, so bits 4-7 = 0001
    # Vertical mirroring (bit 0 = 0) + mapper low = 0x10
    flags6 = 0x10  # Vertical mirroring, mapper bits 0-3 = 0001
    header += bytes([flags6])

    # Byte 7: Flags 7
    # Bit 0-3: Reserved (usually 0 for NES 1.0)
    # Bit 4-7: Mapper high nibble (bits 4-7)
    # For mapper 1: high nibble = 0000, so bits 4-7 = 0000
    flags7 = 0x00  # Mapper high nibble = 0000
    header += bytes([flags7])

    # Bytes 8-15: Padding (zeros for NES 1.0)
    header += bytes([0x00] * 8)

    return bytes(header)


def build_rom(output_path):
    """Build complete .nes ROM file"""
    # Build header (32KB PRG ROM for MMC1)
    header = build_ines_header(prg_size_kb=32, chr_size_kb=0, mapper=1)

    # Build PRG ROM
    prg = build_prg_rom()

    # Write to file
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(prg)

    return len(header) + len(prg)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, 'test_mmc1.nes')

    print(f"Building test_mmc1.nes (MMC1 mapper)...")
    size = build_rom(output_path)
    print(f"Output: {output_path}")
    print(f"Size: {size} bytes ({size // 1024} KB)")

    # Verify header
    print("\niNES Header:")
    with open(output_path, 'rb') as f:
        header = f.read(16)
        print(f"  Magic: {header[0:4]}")
        print(f"  PRG size: {header[4]} x 16KB = {header[4] * 16}KB")
        print(f"  CHR size: {header[5]} x 8KB = {header[5] * 8}KB")
        print(f"  Flags 6: 0x{header[6]:02X}")
        print(f"  Flags 7: 0x{header[7]:02X}")
        mapper = ((header[7] >> 4) & 0x0F) | (header[6] >> 4)
        print(f"  Mapper: {mapper} (MMC1)")

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
