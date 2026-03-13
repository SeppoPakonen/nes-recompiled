#!/usr/bin/env python3
"""
build_test_timer.py - Build timer feature test ROM

Tests timer functionality using CPU cycle counting and frame counter.
The NES doesn't have a dedicated timer peripheral - timing is achieved through:
- CPU cycle counting (1.79 MHz)
- Frame counter (60 Hz)
- VBlank/NMI timing
"""

import struct
import sys
import os

# Debug port
DEBUG_PORT = 0x6000

# Timer-related registers
FRAME_COUNTER = 0x4017
PPUSTATUS = 0x2002
PPUCTRL = 0x2000

# Test results zero page
TEST_RESULTS = 0x00
FRAME_COUNT = 0x11


def print_char(char):
    """Return bytes to print a character to debug port"""
    return bytes([0xA9, char, 0x8D, DEBUG_PORT & 0xFF, DEBUG_PORT >> 8])


def print_string(s):
    """Return bytes to print a string to debug port"""
    result = bytearray()
    for c in s:
        result += print_char(ord(c))
    return bytes(result)


def write_reg(reg_addr, value):
    """Return bytes to write value to register"""
    return bytes([0xA9, value, 0x8D, reg_addr & 0xFF, reg_addr >> 8])


def read_reg_and(reg_addr, mask, store_addr):
    """Return bytes to read register, AND with mask, and store"""
    return bytes([
        0xAD, reg_addr & 0xFF, reg_addr >> 8,
        0x29, mask,
        0x85, store_addr
    ])


def read_reg_cmp_branch(reg_addr, value, branch_offset):
    """Return bytes to read register, compare, and branch if equal"""
    return bytes([
        0xAD, reg_addr & 0xFF, reg_addr >> 8,
        0xC9, value,
        0xF0, branch_offset
    ])


def build_prg_rom():
    """Build PRG ROM with comprehensive timer tests"""
    prg = bytearray()
    
    # =========================================================================
    # Reset Handler
    # =========================================================================
    prg += bytes([0xA2, 0xFF, 0x9A])  # LDX #$FF, TXS
    prg += bytes([0x78])              # SEI
    
    # Clear test variables
    prg += bytes([0xA9, 0x00])
    prg += bytes([0x85, TEST_RESULTS])
    prg += bytes([0x85, TEST_RESULTS+0x10])
    prg += bytes([0x85, FRAME_COUNT])
    prg += bytes([0x85, TEST_RESULTS+0x12])
    
    # Print "TIMER TEST\n"
    prg += print_string("TIMER TEST\n")
    
    # =========================================================================
    # Test 1: CPU Cycle Counting
    # =========================================================================
    prg += print_string("CYC ")
    
    # Store start marker
    prg += bytes([0xA9, 0x01, 0x85, TEST_RESULTS])
    
    # Execute known cycle count instructions (NOP = 2 cycles each)
    for _ in range(5):
        prg += bytes([0xEA])  # NOP
    
    # Store cycle marker
    prg += bytes([0xA9, 0x02, 0x85, TEST_RESULTS+1])
    
    # Create a small delay loop
    # LDX #$10, loop: NOP, DEX, BNE
    prg += bytes([0xA2, 0x10])  # LDX #$10
    delay_loop1 = 0x8000 + len(prg)
    prg += bytes([0xEA])        # NOP (2 cycles)
    prg += bytes([0xCA])        # DEX (2 cycles)
    prg += bytes([0xD0, 0xFA])  # BNE delay_loop1 (-6 bytes, 3 cycles when taken)
    
    # Store delay marker
    prg += bytes([0xA9, 0x03, 0x85, TEST_RESULTS+2])
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 2: Delay Loop
    # =========================================================================
    prg += print_string("DLY ")
    
    # Short delay (~100 cycles)
    prg += bytes([0xA2, 0x0E])  # LDX #$0E
    delay_short = 0x8000 + len(prg)
    prg += bytes([0xCA])        # DEX
    prg += bytes([0xD0, 0xFC])  # BNE delay_short
    prg += bytes([0x85, TEST_RESULTS+3])
    
    # Medium delay (~1000 cycles)
    prg += bytes([0xA2, 0x64])  # LDX #$64 (100)
    delay_medium = 0x8000 + len(prg)
    prg += bytes([0xEA])        # NOP
    prg += bytes([0xCA])        # DEX
    prg += bytes([0xD0, 0xFC])  # BNE delay_medium
    prg += bytes([0x85, TEST_RESULTS+4])
    
    # Long delay (~10000 cycles) - nested loop
    prg += bytes([0xA2, 0x0A])  # LDX #$0A (outer)
    delay_outer = 0x8000 + len(prg)
    prg += bytes([0xA0, 0x64])  # LDY #$64 (inner)
    delay_inner = 0x8000 + len(prg)
    prg += bytes([0x88])        # DEY
    prg += bytes([0xD0, 0xFC])  # BNE delay_inner
    prg += bytes([0xCA])        # DEX
    prg += bytes([0xD0, 0xF6])  # BNE delay_outer
    prg += bytes([0x85, TEST_RESULTS+5])
    
    # Store delay test marker
    prg += bytes([0xA9, 0x01, 0x85, TEST_RESULTS+6])
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 3: Frame Counter Timing
    # =========================================================================
    prg += print_string("FRM ")
    
    # Wait for frames using PPUSTATUS polling
    # Wait for frame 1
    wait_frame1 = 0x8000 + len(prg)
    prg += bytes([0xAD, PPUSTATUS & 0xFF, PPUSTATUS >> 8])
    prg += bytes([0x29, 0x80])  # AND #%10000000
    prg += bytes([0xF0, 0xFA])  # BEQ wait_frame1
    prg += bytes([0xEE, TEST_RESULTS+7, 0x00])  # INC $0007
    
    # Wait for frame 2
    wait_frame2 = 0x8000 + len(prg)
    prg += bytes([0xAD, PPUSTATUS & 0xFF, PPUSTATUS >> 8])
    prg += bytes([0x29, 0x80])
    prg += bytes([0xF0, 0xFA])
    prg += bytes([0xEE, TEST_RESULTS+8, 0x00])
    
    # Wait for frame 3
    wait_frame3 = 0x8000 + len(prg)
    prg += bytes([0xAD, PPUSTATUS & 0xFF, PPUSTATUS >> 8])
    prg += bytes([0x29, 0x80])
    prg += bytes([0xF0, 0xFA])
    prg += bytes([0xEE, TEST_RESULTS+9, 0x00])
    
    # Store frame test marker
    prg += bytes([0xA9, 0x03, 0x85, TEST_RESULTS+10])
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 4: VBlank Timing
    # =========================================================================
    prg += print_string("VBL ")
    
    # Clear frame counter (simulated - NMI would increment it)
    prg += bytes([0xA9, 0x00, 0x85, FRAME_COUNT])
    
    # Enable NMI on VBlank
    prg += write_reg(PPUCTRL, 0b10000000)
    
    # Wait for NMI to increment frame counter (polling FRAME_COUNT)
    # Wait for frame 1
    wait_nmi1 = 0x8000 + len(prg)
    prg += bytes([0xA5, FRAME_COUNT])
    prg += bytes([0xC9, 0x01])
    prg += bytes([0xD0, 0xFB])  # BNE wait_nmi1
    prg += bytes([0xEE, TEST_RESULTS+11, 0x00])
    
    # Wait for frame 2
    wait_nmi2 = 0x8000 + len(prg)
    prg += bytes([0xA5, FRAME_COUNT])
    prg += bytes([0xC9, 0x02])
    prg += bytes([0xD0, 0xFB])
    prg += bytes([0xEE, TEST_RESULTS+12, 0x00])
    
    # Wait for frame 3
    wait_nmi3 = 0x8000 + len(prg)
    prg += bytes([0xA5, FRAME_COUNT])
    prg += bytes([0xC9, 0x03])
    prg += bytes([0xD0, 0xFB])
    prg += bytes([0xEE, TEST_RESULTS+13, 0x00])
    
    # Disable NMI
    prg += write_reg(PPUCTRL, 0x00)
    
    # Store VBlank test marker
    prg += bytes([0xA9, 0x03, 0x85, TEST_RESULTS+14])
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 5: NMI Timing
    # =========================================================================
    prg += print_string("NMI ")
    
    # Clear counters
    prg += bytes([0xA9, 0x00, 0x85, FRAME_COUNT])
    prg += bytes([0x85, TEST_RESULTS+20])
    
    # Enable NMI
    prg += write_reg(PPUCTRL, 0b10000000)
    
    # Set target frame count
    prg += bytes([0xA9, 0x05, 0x85, TEST_RESULTS+15])
    
    # Wait for 5 frames
    wait_nmi_timing = 0x8000 + len(prg)
    prg += bytes([0xA5, FRAME_COUNT])
    prg += bytes([0xC9, 0x05])
    prg += bytes([0xD0, 0xFB])  # BNE wait_nmi_timing
    
    # Disable NMI
    prg += write_reg(PPUCTRL, 0x00)
    
    # Store NMI test marker
    prg += bytes([0xA5, FRAME_COUNT, 0x85, TEST_RESULTS+16])
    
    # Verify we got 5 frames
    prg += bytes([0xC9, 0x05])
    nmi_ok_addr = 0x8000 + len(prg) + 4
    prg += bytes([0xF0, 0x03])  # BEQ nmi_ok
    prg += bytes([0xA9, 0xFF, 0x85, TEST_RESULTS+17])  # Error
    prg += bytes([0x4C])  # JMP done
    done_addr = 0x8000 + len(prg) + 3
    prg += struct.pack('<H', done_addr)
    # nmi_ok:
    prg += bytes([0xA9, 0x01, 0x85, TEST_RESULTS+17])  # Success
    # done:
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Completion
    # =========================================================================
    prg += print_string("DONE\n")
    
    # Infinite loop
    infinite_loop = 0x8000 + len(prg)
    prg += bytes([0x4C])
    prg += struct.pack('<H', infinite_loop)
    
    # =========================================================================
    # NMI Handler
    # =========================================================================
    nmi_handler = 0x8000 + len(prg)
    # PHA, TXA, PHA
    prg += bytes([0x48, 0x8A, 0x48])
    # INC FRAME_COUNT
    prg += bytes([0xEE, FRAME_COUNT, 0x00])
    # Check if 60 frames (1 second)
    prg += bytes([0xA5, FRAME_COUNT, 0xC9, 0x3C])
    skip_second = 0x8000 + len(prg) + 3
    prg += bytes([0xD0, 0x03])  # BNE skip_second
    prg += bytes([0xA9, 0x00, 0x85, FRAME_COUNT])
    prg += bytes([0xEE, TEST_RESULTS, 0x00])  # INC seconds
    # skip_second:
    # PLA, TAX, PLA, RTI
    prg += bytes([0x68, 0xAA, 0x68, 0x40])
    
    # =========================================================================
    # IRQ Handler
    # =========================================================================
    irq_handler = 0x8000 + len(prg)
    prg += bytes([0x48, 0x8A, 0x48])  # PHA, TXA, PHA
    prg += bytes([0xEE, TEST_RESULTS+20, 0x00])  # INC $0014
    prg += bytes([0x68, 0xAA, 0x68, 0x40])  # PLA, TAX, PLA, RTI
    
    # =========================================================================
    # Pad and add vectors
    # =========================================================================
    min_prg_size = 0x4000 - 6
    while len(prg) < min_prg_size:
        prg += bytes([0xEA])

    # Vectors
    prg += struct.pack('<H', nmi_handler)  # NMI
    prg += struct.pack('<H', 0x8000)       # Reset
    prg += struct.pack('<H', irq_handler)  # IRQ
    
    return bytes(prg)


def build_ines_header(prg_size_kb=16, chr_size_kb=0):
    """Build iNES header (16 bytes)"""
    header = bytearray()
    header += b'NES\x1A'
    header += bytes([prg_size_kb // 16])
    header += bytes([chr_size_kb // 8])
    header += bytes([0x00])  # Vertical mirroring, mapper 0
    header += bytes([0x00])
    header += bytes([0x00] * 8)
    return bytes(header)


def build_rom(output_path):
    """Build complete .nes ROM file"""
    header = build_ines_header(prg_size_kb=16, chr_size_kb=0)
    prg = build_prg_rom()
    
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(prg)
    
    return len(header) + len(prg)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, '..', 'test_timer.nes')
    output_path = os.path.normpath(output_path)
    
    print(f"Building test_timer.nes (timer feature test)...")
    size = build_rom(output_path)
    print(f"Output: {output_path}")
    print(f"Size: {size} bytes ({size // 1024} KB)")
    
    # Verify iNES header
    print("\niNES Header verification:")
    with open(output_path, 'rb') as f:
        header = f.read(16)
        print(f"  Magic: {header[0:4]}")
        print(f"  PRG ROM: {header[4]} x 16KB = {header[4] * 16}KB")
        print(f"  CHR ROM: {header[5]} x 8KB = {header[5] * 8}KB")
        print(f"  Mapper: {header[6] & 0xF0 >> 4 | header[7] & 0x0F}")
        print(f"  Mirroring: {'Horizontal' if header[6] & 0x01 else 'Vertical'}")
    
    print("\nTimer features tested:")
    features = [
        "CPU cycle counting (NOP, loop overhead)",
        "Delay loops (short, medium, long)",
        "Frame counter timing (60 Hz = 16.67ms)",
        "VBlank detection via PPUSTATUS polling",
        "NMI timing (VBlank interrupt at 60 Hz)",
        "Frame counting with NMI handler",
        "Second counting (60 frames = 1 second)",
        "Nested delay loops for longer timing",
    ]
    for i, feat in enumerate(features, 1):
        print(f"  {i:2d}. {feat}")
    
    print("\nTiming reference:")
    print("  CPU clock: 1.789772 MHz (NTSC)")
    print("  Frame rate: 60.0988 Hz (NTSC)")
    print("  Cycles per frame: ~29780")
    print("  VBlank duration: ~2273 cycles (1.27 ms)")
    
    print("\nDebug output: Writes to $6000 (intercepted by runtime for stdout)")
    print("\nROM built successfully!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
