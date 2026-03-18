#!/usr/bin/env python3
"""
build_test_mmc1_runtime_bank.py - Build MMC1 runtime bank switching test ROM

Creates a test ROM that demonstrates the limitation of static analysis for MMC1:
- Same address (0xB5EC) has different code in different banks
- Bank is switched at runtime via MMC1 registers
- Static analysis cannot determine which bank is active at each JSR

This reproduces the Parasol Stars issue where palette code is in Bank 0,
but the analyzer needs to track MMC1 state to know when Bank 0 is active.

ROM Structure (64KB = 4 x 16KB banks):
- Bank 0 (file offset 0x0000-0x3FFF, CPU $8000-$BFFF when mapped): 
  At 0xB5EC: LDA #$01; STA $0000; RTS
- Bank 1 (file offset 0x4000-0x7FFF, CPU $8000-$BFFF when mapped):
  At 0xB5EC: LDA #$02; STA $0000; RTS
- Bank 2 (file offset 0x8000-0xBFFF, CPU $8000-$BFFF when mapped):
  At 0xB5EC: LDA #$03; STA $0000; RTS
- Bank 3 (file offset 0xC000-0xFFFF, CPU $C000-$FFFF ALWAYS):
  At 0xB5EC: LDA #$04; STA $0000; RTS
  Main code and vectors at $C000-$FFFF

For MMC1:
- $8000-$BFFF: Switchable (banks 0-2)
- $C000-$FFFF: Fixed to last bank (bank 3)
"""

import struct
import sys
import os

def build_prg_rom():
    """Build PRG ROM with different code at 0xB5EC in each bank"""
    # 4 banks x 16KB = 64KB total
    prg = bytearray(0x10000)
    
    # Code to put at 0xB5EC in each bank
    # This code writes the bank number to RAM $00 and returns
    def make_bank_code(bank_num):
        return bytes([
            0xA9, bank_num,    # LDA #<bank_num>
            0x85, 0x00,        # STA $0000 (write to RAM)
            0x60               # RTS
        ])
    
    # =========================================================================
    # BANK 0: At 0xB5EC, writes $01 to RAM $00
    # Maps to CPU $8000-$BFFF when MMC1 PRG bank = 0
    # =========================================================================
    bank0_offset = 0x0000
    bank0_code = make_bank_code(0x01)
    prg[bank0_offset + 0x35EC:bank0_offset + 0x35EC + len(bank0_code)] = bank0_code
    for i in range(0x4000):
        if i < 0x35EC or i >= 0x35EC + len(bank0_code):
            prg[bank0_offset + i] = 0xEA  # NOP
    
    # =========================================================================
    # BANK 1: At 0xB5EC, writes $02 to RAM $00
    # Maps to CPU $8000-$BFFF when MMC1 PRG bank = 1
    # =========================================================================
    bank1_offset = 0x4000
    bank1_code = make_bank_code(0x02)
    prg[bank1_offset + 0x35EC:bank1_offset + 0x35EC + len(bank1_code)] = bank1_code
    for i in range(0x4000):
        if i < 0x35EC or i >= 0x35EC + len(bank1_code):
            prg[bank1_offset + i] = 0xEA  # NOP
    
    # =========================================================================
    # BANK 2: At 0xB5EC, writes $03 to RAM $00
    # Maps to CPU $8000-$BFFF when MMC1 PRG bank = 2
    # =========================================================================
    bank2_offset = 0x8000
    bank2_code = make_bank_code(0x03)
    prg[bank2_offset + 0x35EC:bank2_offset + 0x35EC + len(bank2_code)] = bank2_code
    for i in range(0x4000):
        if i < 0x35EC or i >= 0x35EC + len(bank2_code):
            prg[bank2_offset + i] = 0xEA  # NOP
    
    # =========================================================================
    # BANK 3 (FIXED): At 0xB5EC, writes $04 to RAM $00
    # ALWAYS maps to CPU $C000-$FFFF (cannot be switched)
    # Contains main code and reset vector
    # =========================================================================
    bank3_offset = 0xC000
    
    # Code at 0xB5EC in bank 3
    bank3_code = make_bank_code(0x04)
    prg[bank3_offset + 0x35EC:bank3_offset + 0x35EC + len(bank3_code)] = bank3_code
    
    # Main code at 0xC000 (offset 0x0000 in bank 3)
    main_code = bytes([
        0x78,              # SEI (disable interrupts)
        0xD8,              # CLD (clear decimal)
        0xA2, 0xFF,        # LDX #$FF
        0x9A,              # TXS (set stack pointer)
        
        # Initialize RAM
        0xA9, 0x00,        # LDA #$00
        0x85, 0x00,        # STA $0000 (clear test result)
        0x85, 0x01,        # STA $0001 (test step counter)
        
        # ================================================================
        # Test 1: Call 0xB5EC with Bank 0 mapped to $8000-$BFFF
        # ================================================================
        # Switch MMC1 to Bank 0
        0xA9, 0x80,        # LDA #$80 (reset MMC1 shift register)
        0x8D, 0x00, 0x80,  # STA $8000
        
        # Write control register (0x0C = PRG RAM enable, 16KB mode)
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
        
        # Switch PRG bank to Bank 0
        0xA9, 0x80,        # LDA #$80 (reset)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x00,        # LDA #$00 (bits 0-4 = 0)
        0x8D, 0x00, 0xA0,  # STA $A000
        0x8D, 0x00, 0xA0,  # STA $A000
        0x8D, 0x00, 0xA0,  # STA $A000
        0x8D, 0x00, 0xA0,  # STA $A000
        0x8D, 0x00, 0xA0,  # STA $A000
        
        # Call 0xB5EC (should execute Bank 0 code, write $01 to RAM)
        0x20, 0xEC, 0xB5,  # JSR $B5EC
        
        # Store result for Bank 0 test
        0xA5, 0x00,        # LDA $0000 (should be $01)
        0x85, 0x02,        # STA $0002 (result Bank 0)
        
        # ================================================================
        # Test 2: Call 0xB5EC with Bank 1 mapped
        # ================================================================
        0xA9, 0x80,        # LDA #$80 (reset)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x01,        # LDA #$01 (bank 1)
        0x8D, 0x00, 0xA0,  # STA $A000 (x5)
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        
        0x20, 0xEC, 0xB5,  # JSR $B5EC
        0xA5, 0x00,        # LDA $0000 (should be $02)
        0x85, 0x03,        # STA $0003 (result Bank 1)
        
        # ================================================================
        # Test 3: Call 0xB5EC with Bank 2 mapped
        # ================================================================
        0xA9, 0x80,        # LDA #$80 (reset)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x02,        # LDA #$02 (bank 2)
        0x8D, 0x00, 0xA0,  # STA $A000 (x5)
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        
        0x20, 0xEC, 0xB5,  # JSR $B5EC
        0xA5, 0x00,        # LDA $0000 (should be $03)
        0x85, 0x04,        # STA $0004 (result Bank 2)
        
        # ================================================================
        # Test 4: Call 0xB5EC with Bank 3 mapped
        # ================================================================
        0xA9, 0x80,        # LDA #$80 (reset)
        0x8D, 0x00, 0xA0,  # STA $A000
        0xA9, 0x03,        # LDA #$03 (bank 3)
        0x8D, 0x00, 0xA0,  # STA $A000 (x5)
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        0x8D, 0x00, 0xA0,
        
        0x20, 0xEC, 0xB5,  # JSR $B5EC
        0xA5, 0x00,        # LDA $0000 (should be $04)
        0x85, 0x05,        # STA $0005 (result Bank 3)
        
        # Infinite loop (test complete)
        0x4C, 0x6A, 0xC0,  # JMP $C06A (this address)
    ])
    
    # Pad main code to reasonable size
    main_code = main_code + bytes([0xEA] * (0x100 - len(main_code)))
    
    prg[bank3_offset:bank3_offset + len(main_code)] = main_code
    
    # Interrupt vectors at END OF FILE (last 6 bytes)
    # For MMC1, vectors are always at the end of the last bank
    vector_offset = len(prg) - 6
    prg[vector_offset:vector_offset + 6] = bytes([
        0xFA, 0x01,  # NMI vector -> $01FA
        0x00, 0xC0,  # Reset vector -> $C000
        0xFE, 0x01,  # IRQ vector -> $01FE
    ])
    
    return bytes(prg)


def build_ines_header(prg_size_kb=64, chr_size_kb=0, mapper=1):
    """Build iNES header for MMC1 with 64KB PRG ROM (4 banks)"""
    header = bytearray()
    header += b'NES\x1A'
    header += bytes([prg_size_kb // 16])
    header += bytes([chr_size_kb // 8])
    header += bytes([0x10])  # Vertical mirroring, mapper low = 1
    header += bytes([0x00])  # Mapper high = 0
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
    output_path = os.path.join(script_dir, 'test_mmc1_runtime_bank.nes')
    
    print(f"Building test_mmc1_runtime_bank.nes (MMC1 runtime bank switching test)...")
    size = build_rom(output_path)
    print(f"Output: {output_path}")
    print(f"Size: {size} bytes ({size // 1024} KB)")
    
    print("\niNES Header:")
    with open(output_path, 'rb') as f:
        header = f.read(16)
        print(f"  PRG size: {header[4]} x 16KB = {header[4] * 16}KB")
        mapper = ((header[7] >> 4) & 0x0F) | (header[6] >> 4)
        print(f"  Mapper: {mapper} (MMC1)")
    
    print("\nCode at 0xB5EC in each bank:")
    with open(output_path, 'rb') as f:
        for bank in range(4):
            offset = 16 + bank * 0x4000 + 0x35EC
            f.seek(offset)
            data = f.read(8)
            print(f"  Bank {bank}: {data.hex()}")
    
    print("\nROM built successfully!")
    print("\nTest description:")
    print("  - Bank 0 at 0xB5EC: LDA #$01; STA $0000; RTS")
    print("  - Bank 1 at 0xB5EC: LDA #$02; STA $0000; RTS")
    print("  - Bank 2 at 0xB5EC: LDA #$03; STA $0000; RTS")
    print("  - Bank 3 at 0xB5EC: LDA #$04; STA $0000; RTS")
    print("  - Fixed bank (Bank 3): Main code at $C000-$FFFF")
    print("\nMMC1 behavior:")
    print("  - $8000-$BFFF: Switchable (banks 0-2)")
    print("  - $C000-$FFFF: Fixed to Bank 3 (cannot be switched)")
    print("\nExpected behavior:")
    print("  - After Bank 0 call: RAM $0002 = $01")
    print("  - After Bank 1 call: RAM $0003 = $02")
    print("  - After Bank 2 call: RAM $0004 = $03")
    print("  - After Bank 3 call: RAM $0005 = $04")
    print("\nStatic analysis challenge:")
    print("  - All 4 banks have code at 0xB5EC")
    print("  - Which bank is active depends on MMC1 register state")
    print("  - Static analyzer cannot determine bank without tracking MMC1 writes")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
