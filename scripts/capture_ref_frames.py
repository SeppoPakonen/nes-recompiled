#!/usr/bin/env python3
"""
Capture frames from reference emulator for comparison.

This script runs the reference emulator and captures frames for comparison
with our interpreter.

Usage:
    python3 scripts/capture_ref_frames.py roms/a.nes
"""

import subprocess
import os
import sys
from pathlib import Path

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 capture_ref_frames.py <rom_file>")
        sys.exit(1)
    
    rom_path = sys.argv[1]
    
    if not os.path.exists(rom_path):
        print(f"ERROR: ROM not found: {rom_path}")
        sys.exit(1)
    
    # Reference emulator path
    ref_exe = Path.home() / 'Ohjelmat' / 'NES' / 'build' / 'nes'
    
    if not ref_exe.exists():
        print(f"ERROR: Reference emulator not found: {ref_exe}")
        sys.exit(1)
    
    # Output directory
    workspace = Path(__file__).parent.parent
    output_dir = workspace / 'logs' / 'ref_frames'
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Run reference emulator
    # Note: The reference emulator may not have screenshot functionality
    # This script is a placeholder for manual frame capture
    print(f"Reference emulator: {ref_exe}")
    print(f"Output directory: {output_dir}")
    print()
    print("NOTE: The reference emulator may not have automatic screenshot capture.")
    print("You may need to manually capture frames or modify the emulator.")
    print()
    print("To run the reference emulator:")
    print(f"  {ref_exe} {rom_path}")
    print()
    print("Expected output: The game should render properly (not solid gray)")
    print("Our interpreter output: Solid gray screen (RGB 80,80,80)")
    print()
    print("Comparison needed:")
    print("  1. Check if reference emulator renders the game correctly")
    print("  2. Compare PPU register writes between interpreter and reference")
    print("  3. Check CHR ROM loading and banking")
    print("  4. Verify nametable and palette initialization")

if __name__ == '__main__':
    main()
