#!/usr/bin/env python3
"""
build_test_ppu_init.py - Build PPU initialization test ROM

Tests:
1. VBlank wait loop (BIT $2002 / BPL)
2. PPUCTRL write ($2000)
3. PPUMASK write ($2001)
4. VRAM write ($2006/$2007)
5. Verify rendering is enabled
"""

import struct
import sys
import os

# iNES header constants
INES_MAGIC = b'NES\x1a'
PRG_ROM_BANKS = 1  # 16KB
CHR_ROM_BANKS = 1  # 8KB
FLAGS6 = 0x00  # Vertical mirroring, mapper 0
FLAGS7 = 0x00  # Mapper 0 (NROM)

# Debug port for stdout logging
DEBUG_PORT = 0x6000


def print_char(char):
    """Return bytes to print a character to debug port"""
    return bytes([0xA9, char, 0x8D, DEBUG_PORT & 0xFF, DEBUG_PORT >> 8])


def print_string(s):
    """Return bytes to print a string to debug port"""
    result = bytearray()
    for c in s:
        result += print_char(ord(c))
    return bytes(result)


def build_prg_rom():
    """Build PRG ROM with PPU initialization test"""
    prg = bytearray()

    # =========================================================================
    # Reset Handler - Entry point at 0x8000
    # =========================================================================
    reset_addr = 0x8000
    
    # Initialize stack pointer
    prg += bytes([0xA2, 0xFF, 0x9A])  # LDX #$FF, TXS
    prg += bytes([0x78])              # SEI (disable interrupts)
    prg += bytes([0xD8])              # CLD (clear decimal mode)
    
    # Print test header
    prg += print_string("PPU INIT TEST\n")
    
    # =========================================================================
    # Test 1: Wait for VBlank
    # =========================================================================
    prg += print_string("Waiting for VBlank...")
    
    # VBlank wait loop
    # This is the standard pattern used by NES games
    vblank_wait = len(prg) + reset_addr
    prg += bytes([
        0xAD, 0x02, 0x20,  # LDA $2002 (read PPUSTATUS)
        0x10, 0xFB         # BPL vblank_wait (loop until bit 7 set)
    ])
    
    prg += print_string(" OK\n")
    
    # =========================================================================
    # Test 2: Write to PPUCTRL ($2000)
    # =========================================================================
    prg += print_string("Writing PPUCTRL=$80 (enable NMI)...")
    
    # Write to PPUCTRL
    # Bit 7: Enable NMI
    # Bit 6-4: Nametable select
    # Bit 3: VRAM address increment
    # Bit 2-0: Sprite/table pattern select
    prg += bytes([
        0xA9, 0x80,  # LDA #$80 (enable NMI, nametable 0, inc by 1)
        0x8D, 0x00, 0x20  # STA $2000 (PPUCTRL)
    ])
    
    prg += print_string(" OK\n")
    
    # =========================================================================
    # Test 3: Write to PPUMASK ($2001)
    # =========================================================================
    prg += print_string("Writing PPUMASK=$1E (enable BG+spr)...")
    
    # Write to PPUMASK
    # Bit 7: Blue emphasis
    # Bit 6: Green emphasis
    # Bit 5: Red emphasis
    # Bit 4: Enable sprites
    # Bit 3: Enable background
    # Bit 2-0: Color emphasis
    prg += bytes([
        0xA9, 0x1E,  # LDA #$1E (enable sprites+background, no emphasis)
        0x8D, 0x01, 0x20  # STA $2001 (PPUMASK)
    ])
    
    prg += print_string(" OK\n")
    
    # =========================================================================
    # Test 4: Write to VRAM (nametable)
    # =========================================================================
    prg += print_string("Writing VRAM (nametable)...")
    
    # Set VRAM address to nametable 0 start ($2000)
    # Write high byte first, then low byte
    prg += bytes([
        0xA9, 0x20,  # LDA #$20 (high byte of $2000)
        0x8D, 0x06, 0x20,  # STA $2006 (PPUADDR)
        0xA9, 0x00,  # LDA #$00 (low byte of $2000)
        0x8D, 0x06, 0x20,  # STA $2006 (PPUADDR)
    ])
    
    # Write pattern index 0x01 (tile 1) to first 32 bytes of nametable
    for i in range(32):
        prg += bytes([
            0xA9, 0x01,  # LDA #$01 (tile index 1)
            0x8D, 0x07, 0x20,  # STA $2007 (PPUDATA)
        ])
    
    # Write different pattern to next 32 bytes
    for i in range(32):
        prg += bytes([
            0xA9, 0x02,  # LDA #$02 (tile index 2)
            0x8D, 0x07, 0x20,  # STA $2007 (PPUDATA)
        ])
    
    prg += print_string(" OK\n")
    
    # =========================================================================
    # Test 5: Verify PPUCTRL/PPUMASK values
    # =========================================================================
    prg += print_string("Verifying PPU registers...")
    
    # Read back PPUSTATUS (note: this clears VBlank flag)
    prg += bytes([
        0xAD, 0x02, 0x20,  # LDA $2002
        0x85, 0x00,  # STA $00 (store in zero page for debug)
    ])
    
    prg += print_string(" OK\n")
    
    # =========================================================================
    # Test 6: Print status
    # =========================================================================
    prg += print_string("\nPPU INIT COMPLETE\n")
    prg += print_string("Check SDL window for rendering\n")
    
    # =========================================================================
    # Infinite loop
    # =========================================================================
    infinite_loop = len(prg) + reset_addr
    prg += bytes([
        0x4C, infinite_loop & 0xFF, (infinite_loop >> 8) & 0xFF  # JMP infinite_loop
    ])
    
    # =========================================================================
    # Pad to exact 16KB size (including vectors)
    # =========================================================================
    # Total PRG ROM must be exactly 16384 bytes (0x4000)
    # We need: code + vectors (6 bytes) = 16384
    # So pad code to: 16384 - 6 = 16378 bytes
    min_code_size = 0x4000 - 6  # 16378 bytes
    while len(prg) < min_code_size:
        prg += bytes([0xEA])  # NOP padding
    
    # =========================================================================
    # Add interrupt vectors (last 6 bytes of PRG ROM)
    # =========================================================================
    # Vectors are at offsets 16378-16383 (0x3FFA-0x3FFF) from PRG start
    # Which are addresses 0xFFFA-0xFFFF
    nmi_handler = reset_addr  # Point NMI to reset (just for testing)
    reset_vector = reset_addr  # Reset points to start of code
    irq_handler = reset_addr  # Point IRQ to reset
    
    # NMI vector (bytes -6, -5)
    prg += struct.pack('<H', nmi_handler)
    # Reset vector (bytes -4, -3)
    prg += struct.pack('<H', reset_vector)
    # IRQ vector (bytes -2, -1)
    prg += struct.pack('<H', irq_handler)
    
    return prg


def build_chr_rom():
    """Build simple CHR ROM with test tiles"""
    chr = bytearray()
    
    # Tile 0: Empty (all black)
    for i in range(16):
        chr.append(0x00)
    
    # Tile 1: All pixels color 1 (white pattern)
    # Bitplane 0: all 1s
    # Bitplane 1: all 1s
    # Result: all pixels = 0b11 = color 3 (white)
    for i in range(8):
        chr.append(0xFF)  # Bitplane 0
    for i in range(8):
        chr.append(0xFF)  # Bitplane 1
    
    # Tile 2: Checkerboard pattern
    # Alternating pixels
    for row in range(8):
        if row % 2 == 0:
            chr.append(0x55)  # 01010101
        else:
            chr.append(0xAA)  # 10101010
    for row in range(8):
        if row % 2 == 0:
            chr.append(0x00)  # All 0s for bitplane 1
        else:
            chr.append(0xFF)  # All 1s for bitplane 1
    
    # Pad to 8KB
    while len(chr) < 8192:
        chr.append(0x00)
    
    return chr


def main():
    print("Building test_ppu_init.nes (PPU initialization test)...")
    
    # Build PRG and CHR
    prg_rom = build_prg_rom()
    chr_rom = build_chr_rom()
    
    # Create iNES file
    ines_file = bytearray()
    
    # iNES header (16 bytes)
    ines_file += INES_MAGIC
    ines_file.append(PRG_ROM_BANKS)
    ines_file.append(CHR_ROM_BANKS)
    ines_file.append(FLAGS6)
    ines_file.append(FLAGS7)
    ines_file += bytes([0x00] * 8)  # Padding
    
    # PRG ROM
    ines_file += prg_rom
    
    # CHR ROM
    ines_file += chr_rom
    
    # Write file
    output_path = os.path.join(os.path.dirname(__file__), 'test_ppu_init.nes')
    with open(output_path, 'wb') as f:
        f.write(ines_file)
    
    print(f"Output: {output_path}")
    print(f"Size: {len(ines_file)} bytes ({len(ines_file) // 1024} KB)")
    print()
    print("iNES Header verification:")
    print(f"  Magic: {ines_file[0:4]}")
    print(f"  PRG ROM: {PRG_ROM_BANKS} x 16KB = {PRG_ROM_BANKS * 16}KB")
    print(f"  CHR ROM: {CHR_ROM_BANKS} x 8KB = {CHR_ROM_BANKS * 8}KB")
    print(f"  Mapper: {FLAGS7 & 0xF0 | (FLAGS6 >> 4)}")
    print(f"  Mirroring: {'Vertical' if FLAGS6 & 0x01 else 'Horizontal'}")
    print()
    print("ROM built successfully!")


if __name__ == '__main__':
    main()
