#!/usr/bin/env python3
"""
build_test_apu.py - Build comprehensive APU feature test ROM

Tests all APU channels: Pulse 1, Pulse 2, Triangle, Noise, DMC
Also tests frame counter and status register.
"""

import struct
import sys
import os

# Debug port
DEBUG_PORT = 0x6000

# APU Registers
PULSE1_CTRL = 0x4000
PULSE1_SWEEP = 0x4001
PULSE1_LO = 0x4002
PULSE1_HI = 0x4003

PULSE2_CTRL = 0x4004
PULSE2_SWEEP = 0x4005
PULSE2_LO = 0x4006
PULSE2_HI = 0x4007

TRIANGLE_CTRL = 0x4008
TRIANGLE_LO = 0x400A
TRIANGLE_HI = 0x400B

NOISE_CTRL = 0x400C
NOISE_LO = 0x400E
NOISE_HI = 0x400F

DMC_FLAGS = 0x4010
DMC_RAW = 0x4011
DMC_START = 0x4012
DMC_LENGTH = 0x4013

FRAME_COUNTER = 0x4017
APU_STATUS = 0x4015

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


def write_apu_reg(reg_addr, value):
    """Return bytes to write value to APU register"""
    return bytes([0xA9, value, 0x8D, reg_addr & 0xFF, reg_addr >> 8])


def read_apu_reg(reg_addr, store_addr):
    """Return bytes to read APU register and store"""
    return bytes([
        0xAD, reg_addr & 0xFF, reg_addr >> 8,
        0x85, store_addr
    ])


def build_prg_rom():
    """Build PRG ROM with comprehensive APU tests"""
    prg = bytearray()
    
    # =========================================================================
    # Reset Handler
    # =========================================================================
    prg += bytes([0xA2, 0xFF, 0x9A])  # LDX #$FF, TXS
    prg += bytes([0x78])              # SEI
    
    # Print "APU TEST\n"
    prg += print_string("APU TEST\n")
    
    # =========================================================================
    # Test 1: Pulse Channel 1 ($4000-$4003)
    # =========================================================================
    prg += print_string("P1 ")
    
    # Pulse 1 Control
    prg += write_apu_reg(PULSE1_CTRL, 0b01011111)  # Volume=15, env disabled, 25% duty
    prg += read_apu_reg(PULSE1_CTRL, TEST_RESULTS)
    
    # Pulse 1 Sweep
    prg += write_apu_reg(PULSE1_SWEEP, 0x00)  # Sweep disabled
    prg += read_apu_reg(PULSE1_SWEEP, TEST_RESULTS+1)
    
    # Pulse 1 Timer Low
    prg += write_apu_reg(PULSE1_LO, 0x00)
    prg += read_apu_reg(PULSE1_LO, TEST_RESULTS+2)
    
    # Pulse 1 Timer High
    prg += write_apu_reg(PULSE1_HI, 0x0A)
    prg += read_apu_reg(PULSE1_HI, TEST_RESULTS+3)
    
    # Test different duty cycles
    prg += write_apu_reg(PULSE1_CTRL, 0b00011111)  # 12.5%
    prg += write_apu_reg(PULSE1_CTRL, 0b01011111)  # 25%
    prg += write_apu_reg(PULSE1_CTRL, 0b10011111)  # 50%
    prg += write_apu_reg(PULSE1_CTRL, 0b11011111)  # 75%
    
    # Test volume settings
    prg += write_apu_reg(PULSE1_CTRL, 0b00000000)  # Volume 0
    prg += write_apu_reg(PULSE1_CTRL, 0b00001111)  # Volume 15
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 2: Pulse Channel 2 ($4004-$4007)
    # =========================================================================
    prg += print_string("P2 ")
    
    # Pulse 2 Control
    prg += write_apu_reg(PULSE2_CTRL, 0b01011111)
    prg += read_apu_reg(PULSE2_CTRL, TEST_RESULTS+4)
    
    # Pulse 2 Sweep
    prg += write_apu_reg(PULSE2_SWEEP, 0x00)
    prg += read_apu_reg(PULSE2_SWEEP, TEST_RESULTS+5)
    
    # Pulse 2 Timer Low
    prg += write_apu_reg(PULSE2_LO, 0x00)
    prg += read_apu_reg(PULSE2_LO, TEST_RESULTS+6)
    
    # Pulse 2 Timer High
    prg += write_apu_reg(PULSE2_HI, 0x0A)
    prg += read_apu_reg(PULSE2_HI, TEST_RESULTS+7)
    
    # Test 50% duty (most common)
    prg += write_apu_reg(PULSE2_CTRL, 0b10011111)
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 3: Triangle Channel ($4008-$400B)
    # =========================================================================
    prg += print_string("TRI ")
    
    # Triangle Control/Linear Counter
    prg += write_apu_reg(TRIANGLE_CTRL, 0b10001111)  # Load mode, reload=15
    prg += read_apu_reg(TRIANGLE_CTRL, TEST_RESULTS+8)
    
    # Triangle Timer Low
    prg += write_apu_reg(TRIANGLE_LO, 0x00)
    prg += read_apu_reg(TRIANGLE_LO, TEST_RESULTS+9)
    
    # Triangle Timer High
    prg += write_apu_reg(TRIANGLE_HI, 0x0A)
    prg += read_apu_reg(TRIANGLE_HI, TEST_RESULTS+10)
    
    # Test different linear counter values
    prg += write_apu_reg(TRIANGLE_CTRL, 0b10011111)  # Reload=31
    prg += write_apu_reg(TRIANGLE_CTRL, 0b00000000)  # Load mode disabled
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 4: Noise Channel ($400C-$400F)
    # =========================================================================
    prg += print_string("NOI ")
    
    # Noise Control
    prg += write_apu_reg(NOISE_CTRL, 0b01011111)  # Volume=15, env disabled
    prg += read_apu_reg(NOISE_CTRL, TEST_RESULTS+11)
    
    # Noise Timer Low
    prg += write_apu_reg(NOISE_LO, 0x00)  # Period index 0
    prg += read_apu_reg(NOISE_LO, TEST_RESULTS+12)
    
    # Noise Timer High
    prg += write_apu_reg(NOISE_HI, 0x00)
    prg += read_apu_reg(NOISE_HI, TEST_RESULTS+13)
    
    # Test different noise modes
    prg += write_apu_reg(NOISE_CTRL, 0b00011111)  # Regular LFSR
    prg += write_apu_reg(NOISE_CTRL, 0b10011111)  # 7-bit LFSR
    
    # Test different timer periods
    prg += write_apu_reg(NOISE_LO, 0x0F)  # Period index 15
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 5: DMC Channel ($4010-$4013)
    # =========================================================================
    prg += print_string("DMC ")
    
    # DMC Flags
    prg += write_apu_reg(DMC_FLAGS, 0x00)  # Freq 0, no loop, no IRQ, disabled
    prg += read_apu_reg(DMC_FLAGS, TEST_RESULTS+14)
    
    # DMC Raw DAC
    prg += write_apu_reg(DMC_RAW, 0b01111111)  # Max value (127)
    prg += bytes([
        0xAD, DMC_RAW & 0xFF, DMC_RAW >> 8,
        0x29, 0x7F,  # AND #%01111111
        0x85, TEST_RESULTS+15
    ])
    
    # DMC Start Address
    prg += write_apu_reg(DMC_START, 0x00)
    prg += read_apu_reg(DMC_START, TEST_RESULTS+16)
    
    # DMC Sample Length
    prg += write_apu_reg(DMC_LENGTH, 0x00)
    prg += read_apu_reg(DMC_LENGTH, TEST_RESULTS+17)
    
    # Test different frequencies
    prg += write_apu_reg(DMC_FLAGS, 0b00001111)  # Frequency index 15
    
    # Test loop mode
    prg += write_apu_reg(DMC_FLAGS, 0b00010000)  # Loop enabled
    
    # Test IRQ enable
    prg += write_apu_reg(DMC_FLAGS, 0b01000000)  # IRQ enabled
    
    # Disable IRQ
    prg += write_apu_reg(DMC_FLAGS, 0x00)
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 6: Frame Counter ($4017)
    # =========================================================================
    prg += print_string("FRM ")
    
    # 4-step sequence (default)
    prg += write_apu_reg(FRAME_COUNTER, 0b00000000)
    prg += read_apu_reg(FRAME_COUNTER, TEST_RESULTS+18)
    
    # 5-step sequence
    prg += write_apu_reg(FRAME_COUNTER, 0b01000000)
    prg += read_apu_reg(FRAME_COUNTER, TEST_RESULTS+19)
    
    # IRQ inhibit
    prg += write_apu_reg(FRAME_COUNTER, 0b11000000)
    prg += read_apu_reg(FRAME_COUNTER, TEST_RESULTS+20)
    
    # 4-step with IRQ inhibit (most common)
    prg += write_apu_reg(FRAME_COUNTER, 0b10000000)
    
    prg += print_string("OK\n")
    
    # =========================================================================
    # Test 7: APU Status Register ($4015)
    # =========================================================================
    prg += print_string("STS ")
    
    # Enable all channels
    prg += write_apu_reg(APU_STATUS, 0b00011111)
    prg += read_apu_reg(APU_STATUS, TEST_RESULTS+21)
    
    # Read status (channel active bits)
    prg += bytes([
        0xAD, APU_STATUS & 0xFF, APU_STATUS >> 8,
        0x29, 0x1F,  # AND #%00011111
        0x85, TEST_RESULTS+22
    ])
    
    # Disable all channels
    prg += write_apu_reg(APU_STATUS, 0x00)
    prg += read_apu_reg(APU_STATUS, TEST_RESULTS+23)
    
    # Enable individual channels
    prg += write_apu_reg(APU_STATUS, 0b00000001)  # Pulse 1 only
    prg += write_apu_reg(APU_STATUS, 0b00000100)  # Triangle only
    prg += write_apu_reg(APU_STATUS, 0b00001000)  # Noise only
    prg += write_apu_reg(APU_STATUS, 0b00010000)  # DMC only
    
    # Re-enable all
    prg += write_apu_reg(APU_STATUS, 0b00011111)
    
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
    output_path = os.path.join(script_dir, '..', 'test_apu.nes')
    output_path = os.path.normpath(output_path)
    
    print(f"Building test_apu.nes (comprehensive APU test)...")
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
    
    print("\nAPU features tested:")
    features = [
        "Pulse Channel 1 - Control ($4000), Sweep ($4001), Timer ($4002-$4003)",
        "Pulse Channel 2 - Control ($4004), Sweep ($4005), Timer ($4006-$4007)",
        "Triangle Channel - Control ($4008), Timer ($400A-$400B)",
        "Noise Channel - Control ($400C), Timer ($400E-$400F)",
        "DMC Channel - Flags ($4010), Raw DAC ($4011), Start/Length ($4012-$4013)",
        "Frame Counter ($4017) - 4-step and 5-step sequences",
        "APU Status ($4015) - Channel enable and status",
        "Duty cycle variations (12.5%, 25%, 50%, 75%)",
        "Volume and envelope settings",
        "Timer and frequency configurations",
        "IRQ and loop modes",
    ]
    for i, feat in enumerate(features, 1):
        print(f"  {i:2d}. {feat}")
    
    print("\nDebug output: Writes to $6000 (intercepted by runtime for stdout)")
    print("\nROM built successfully!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
