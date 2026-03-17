#!/usr/bin/env python3
"""
build_test_mmc1_palette.py - Build MMC1 palette bank test ROM

Creates a minimal iNES ROM that tests MMC1 bank switching for palette initialization.
This reproduces the Parasol Stars issue where palette code is in Bank 0 at 0xB5EC,
but the analyzer was only looking at Bank 1 at that address.

ROM Structure:
- Bank 0 (0x8000-0xBFFF): Contains palette initialization at 0xB5EC
- Bank 1 (0x8000-0xBFFF): Contains different code at 0xB5EC (just SEC)
- Fixed bank (0xC000-0xFFFF): Reset vector and main code in all banks
"""

import struct
import sys
import os

def build_prg_rom():
    """Build PRG ROM with palette initialization in Bank 0"""
    # Each bank is 16KB (0x4000 bytes)
    # Bank 0: 0x0000-0x3FFF (maps to $8000-$BFFF)
    # Bank 1: 0x4000-0x7FFF (maps to $8000-$BFFF when switched)
    # Fixed:  0x8000-0xBFFF (maps to $C000-$FFFF always)
    
    prg = bytearray(0x10000)  # 64KB total (4 banks, but we use 2)
    
    # =========================================================================
    # BANK 0: Palette initialization at 0xB5EC (offset 0x35EC in bank)
    # =========================================================================
    bank0_offset = 0x0000
    
    # Palette init code at 0xB5EC (offset 0x35EC in bank 0)
    palette_init_addr = 0x35EC
    palette_code = bytes([
        0xA9, 0x3F,        # LDA #$3F (palette address high)
        0x8D, 0x06, 0x20,  # STA $2006 (PPUADDR)
        0xA9, 0x00,        # LDA #$00 (palette address low)
        0x8D, 0x06, 0x20,  # STA $2006 (PPUADDR)
        0xA9, 0x0F,        # LDA #$0F (black color)
        0x8D, 0x07, 0x20,  # STA $2007 (PPUDATA - palette[0])
        0xA9, 0x30,        # LDA #$30 (white color)
        0x8D, 0x07, 0x20,  # STA $2007 (PPUDATA - palette[1])
        0xA9, 0x16,        # LDA #$16 (red color)
        0x8D, 0x07, 0x20,  # STA $2007 (PPUDATA - palette[2])
        0x60               # RTS
    ])
    prg[bank0_offset + palette_init_addr:bank0_offset + palette_init_addr + len(palette_code)] = palette_code
    
    # Fill rest of bank 0 with NOPs
    for i in range(0x4000):
        if i < palette_init_addr or i >= palette_init_addr + len(palette_code):
            prg[bank0_offset + i] = 0xEA  # NOP
    
    # =========================================================================
    # BANK 1: Different code at same address 0xB5EC (just SEC instruction)
    # =========================================================================
    bank1_offset = 0x4000
    
    # At 0xB5EC in bank 1, just put SEC (different from palette init!)
    sec_addr = 0x35EC
    prg[bank1_offset + sec_addr] = 0x38  # SEC
    
    # Fill rest of bank 1 with NOPs
    for i in range(0x4000):
        if i != sec_addr:
            prg[bank1_offset + i] = 0xEA  # NOP
    
    # =========================================================================
    # FIXED BANK (0xC000-$FFFF): Main code and vectors
    # =========================================================================
    fixed_offset = 0x8000  # Maps to $C000-$FFFF
    
    # Reset handler at 0xC000 (offset 0x0000 in fixed bank)
    reset_addr = 0x0000
    reset_code = bytes([
        0x78,              # SEI (disable interrupts)
        0xD8,              # CLD (clear decimal)
        0xA2, 0xFF,        # LDX #$FF
        0x9A,              # TXS (set stack pointer)
        
        # Initialize PPU (turn off rendering)
        0xA9, 0x00,        # LDA #$00
        0x8D, 0x00, 0x20,  # STA $2000 (PPUCTRL)
        0x8D, 0x01, 0x20,  # STA $2001 (PPUMASK)
        
        # Initialize RAM
        0xA9, 0x00,        # LDA #$00
        0x85, 0x00,        # STA $0000
        0x85, 0x01,        # STA $0001
        
        # Test: Call palette init (should be at 0xB5EC in Bank 0)
        # First, switch to Bank 0 via MMC1
        # MMC1 control register at $8000-$BFFF
        # Write 5 bits serially
        
        # Reset MMC1 shift register
        0xA9, 0x80,        # LDA #$80
        0x8D, 0x00, 0x80,  # STA $8000
        
        # Write control register (0x0C = PRG RAM enable, 16KB mode, vertical mirror)
        # Binary: 01100, write LSB first
        0xA9, 0x00,        # LDA #$00 (bit 0)
        0x8D, 0x00, 0x80,  # STA $8000
        0xA9, 0x00,        # LDA #$00 (bit 1)
        0x8D, 0x00, 0x80,  # STA $8000
        0xA9, 0x01,        # LDA #$01 (bit 2)
        0x8D, 0x00, 0x80,  # STA $8000
        0xA9, 0x01,        # LDA #$01 (bit 3)
        0x8D, 0x00, 0x80,  # STA $8000
        0xA9, 0x00,        # LDA #$00 (bit 4)
        0x8D, 0x00, 0x80,  # STA $8000
        
        # Now switch PRG bank to Bank 0 (write to $A000-$BFFF)
        # Reset shift register
        0xA9, 0x80,        # LDA #$80
        0x8D, 0x00, 0xA0,  # STA $A000
        
        # Write bank 0 (binary 00000)
        0xA9, 0x00,        # LDA #$00 (bit 0)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x00,        # LDA #$00 (bit 1)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x00,        # LDA #$00 (bit 2)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x00,        # LDA #$00 (bit 3)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x00,        # LDA #$00 (bit 4)
        0x8D, 0x00, 0xA0,  # STA $A000
        
        # Now Bank 0 is mapped to $8000-$BFFF
        # Call palette init at 0xB5EC
        0x20, 0xEC, 0xB5,  # JSR $B5EC
        
        # Check if palette was initialized (read back from PPU)
        # Store result in RAM
        0xA9, 0x01,        # LDA #$01 (success marker)
        0x85, 0x02,        # STA $0002
        
        # Infinite loop
        0x4C, 0x18, 0xC0,  # JMP $C018 (this address)
    ])
    prg[fixed_offset + reset_addr:fixed_offset + reset_addr + len(reset_code)] = reset_code
    
    # NMI handler (empty - just RTI)
    nmi_addr = 0x01FA
    prg[fixed_offset + nmi_addr] = 0x40  # RTI
    
    # IRQ handler (empty - just RTI)
    irq_addr = 0x01FE
    prg[fixed_offset + irq_addr] = 0x40  # RTI
    
    # Interrupt vectors at end of fixed bank (0xFFFA-0xFFFF)
    vector_offset = 0x1FFA
    prg[fixed_offset + vector_offset:fixed_offset + vector_offset + 6] = bytes([
        0xFA, 0x01,  # NMI vector -> $01FA
        0x00, 0xC0,  # Reset vector -> $C000
        0xFE, 0x01,  # IRQ vector -> $01FE
    ])
    
    return bytes(prg)


def build_ines_header(prg_size_kb=64, chr_size_kb=0, mapper=1):
    """Build iNES header for MMC1 with 64KB PRG ROM (4 banks)"""
    header = bytearray()
    
    # Magic number
    header += b'NES\x1A'
    
    # PRG ROM size in 16KB units (64KB / 16KB = 4)
    header += bytes([prg_size_kb // 16])
    
    # CHR ROM size in 8KB units
    header += bytes([chr_size_kb // 8])
    
    # Flags 6: Vertical mirroring + mapper low nibble (0001 for mapper 1)
    header += bytes([0x10])
    
    # Flags 7: Mapper high nibble (0000 for mapper 1)
    header += bytes([0x00])
    
    # Padding
    header += bytes([0x00] * 8)
    
    return bytes(header)


def build_rom(output_path):
    """Build complete .nes ROM file"""
    header = build_ines_header(prg_size_kb=64, chr_size_kb=0, mapper=1)
    prg = build_prg_rom()
    
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(prg)
    
    return len(header) + len(prg)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, 'test_mmc1_palette.nes')
    
    print(f"Building test_mmc1_palette.nes (MMC1 palette bank test)...")
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
    
    # Show palette init code location
    print("\nBank 0 at 0xB5EC (palette init):")
    with open(output_path, 'rb') as f:
        f.seek(16 + 0x35EC)  # Header + bank 0 offset
        data = f.read(16)
        for i in range(0, len(data), 16):
            hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
            print(f"  {hex_str}")
    
    print("\nBank 1 at 0xB5EC (SEC only):")
    with open(output_path, 'rb') as f:
        f.seek(16 + 0x4000 + 0x35EC)  # Header + bank 1 offset
        data = f.read(16)
        for i in range(0, len(data), 16):
            hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
            print(f"  {hex_str}")
    
    print("\nROM built successfully!")
    print("\nTest description:")
    print("  - Bank 0 at 0xB5EC: Palette initialization code")
    print("  - Bank 1 at 0xB5EC: Just SEC instruction")
    print("  - Fixed bank: Main code that switches to Bank 0 and calls 0xB5EC")
    print("\nExpected behavior:")
    print("  - Recompiler should analyze BOTH banks at 0xB5EC")
    print("  - Generated code should call palette init when Bank 0 is active")
    print("  - RAM $0002 should be set to $01 if palette init was called")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
