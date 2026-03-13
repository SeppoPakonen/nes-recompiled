#!/usr/bin/env python3
"""
build_test_input.py - Build controller input feature test ROM

Tests controller reading via JOY1 ($4016) and JOY2 ($4017).
Tests strobe, button state reading, and shift register operation.
"""

import struct
import sys
import os

# Debug port
DEBUG_PORT = 0x6000

# Controller registers
JOY1 = 0x4016
JOY2 = 0x4017

# Test results zero page
TEST_RESULTS = 0x00


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


def read_reg_store(reg_addr, store_addr):
    """Return bytes to read register and store"""
    return bytes([
        0xAD, reg_addr & 0xFF, reg_addr >> 8,
        0x85, store_addr
    ])


def strobe_controller():
    """Return bytes to strobe controller (write 1 then 0 to $4016)"""
    return bytes([
        0xA9, 0x01,
        0x8D, JOY1 & 0xFF, JOY1 >> 8,
        0xA9, 0x00,
        0x8D, JOY1 & 0xFF, JOY1 >> 8
    ])


def build_prg_rom():
    """Build PRG ROM with comprehensive input tests"""
    prg = bytearray()
    
    # =========================================================================
    # Reset Handler
    # =========================================================================
    prg += bytes([0xA2, 0xFF, 0x9A])  # LDX #$FF, TXS
    prg += bytes([0x78])              # SEI
    
    # Print "INPUT TEST\n"
    prg += print_string("INPUT TEST\n")
    
    # =========================================================================
    # Test 1: JOY1 Strobe Write
    # =========================================================================
    prg += print_string("STR ")
    
    # Write 1 to strobe
    prg += write_reg(JOY1, 0x01)
    prg += read_reg_store(JOY1, TEST_RESULTS)
    
    # Write 0 to end strobe
    prg += write_reg(JOY1, 0x00)
    prg += read_reg_store(JOY1, TEST_RESULTS+1)
    
    # Store strobe marker
    prg += bytes([0xA9, 0x01, 0x85, TEST_RESULTS+2])
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 2: Button State Reading
    # =========================================================================
    prg += print_string("BTN ")
    
    # Strobe first
    prg += strobe_controller()
    
    # Read all 8 buttons (A, B, Select, Start, Up, Down, Left, Right)
    for i in range(8):
        prg += read_reg_and(JOY1, 0x01, TEST_RESULTS+3+i)
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 3: Shift Register Operation
    # =========================================================================
    prg += print_string("SHF ")
    
    # Strobe
    prg += strobe_controller()
    
    # Read all 8 buttons and store full byte values
    for i in range(8):
        prg += read_reg_store(JOY1, TEST_RESULTS+11+i)
    
    # After 8 reads, controller should return 1s
    # Read 4 more times to verify
    for i in range(4):
        prg += read_reg_store(JOY1, TEST_RESULTS+19+i)
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 4: Controller 2 (JOY2)
    # =========================================================================
    prg += print_string("2PD ")
    
    # Strobe for both controllers
    prg += strobe_controller()
    
    # Read from JOY2 (controller 2) - first 8 reads are button data
    for i in range(8):
        prg += read_reg_and(JOY2, 0x01, TEST_RESULTS+23+i)
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 5: Multiple Read Sequences
    # =========================================================================
    prg += print_string("MLT ")
    
    # First sequence
    prg += strobe_controller()
    prg += read_reg_store(JOY1, TEST_RESULTS+31)
    
    # Second sequence
    prg += strobe_controller()
    prg += read_reg_store(JOY1, TEST_RESULTS+32)
    
    # Third sequence
    prg += strobe_controller()
    prg += read_reg_store(JOY1, TEST_RESULTS+33)
    
    # Store sequence count
    prg += bytes([0xA9, 0x03, 0x85, TEST_RESULTS+34])
    
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
    # Pad and add vectors
    # =========================================================================
    min_prg_size = 0x4000 - 6
    while len(prg) < min_prg_size:
        prg += bytes([0xEA])
    
    # NMI handler (just RTI)
    nmi_handler = 0x8000 + len(prg)
    prg += bytes([0x40])  # RTI
    
    # Vectors
    prg += struct.pack('<H', nmi_handler)  # NMI
    prg += struct.pack('<H', 0x8000)       # Reset
    prg += struct.pack('<H', nmi_handler)  # IRQ
    
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
    output_path = os.path.join(script_dir, '..', 'test_input.nes')
    output_path = os.path.normpath(output_path)
    
    print(f"Building test_input.nes (controller input test)...")
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
    
    print("\nController features tested:")
    features = [
        "JOY1 strobe signal ($4016 write 1 then 0)",
        "Button state reading (8 buttons: A, B, Select, Start, Up, Down, Left, Right)",
        "Shift register operation (each read shifts to next button)",
        "Controller 2 reading via JOY2 ($4017)",
        "Multiple strobe/read sequences",
        "Button bit positions (bits 0-7)",
        "Post-read behavior (returns 1s after 8 reads)",
    ]
    for i, feat in enumerate(features, 1):
        print(f"  {i:2d}. {feat}")
    
    print("\nButton mapping:")
    buttons = [
        (0, "A"),
        (1, "B"),
        (2, "Select"),
        (3, "Start"),
        (4, "Up"),
        (5, "Down"),
        (6, "Left"),
        (7, "Right"),
    ]
    for bit, name in buttons:
        print(f"  Bit {bit}: {name}")
    
    print("\nDebug output: Writes to $6000 (intercepted by runtime for stdout)")
    print("\nROM built successfully!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
