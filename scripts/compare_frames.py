#!/usr/bin/env python3
"""
Frame comparison tool for NES emulator testing.

Captures frames from both the recompiled interpreter and reference emulator,
then compares frame hashes and RGB pixel distributions.

Usage:
    python3 scripts/compare_frames.py roms/a.nes
"""

import subprocess
import os
import hashlib
import sys
from pathlib import Path
from collections import Counter

# Configuration
MAX_FRAMES = 256
TIMEOUT = 60  # seconds per run

def run_and_capture_frames(executable, rom_path, output_dir, extra_args=None):
    """Run emulator and capture frames to output directory."""
    os.makedirs(output_dir, exist_ok=True)
    
    cmd = [executable, rom_path]
    if extra_args:
        cmd.extend(extra_args)
    
    # Add frame capture argument
    cmd.extend(['--screenshot-prefix', f'{output_dir}/frame'])
    
    print(f"Running: {' '.join(cmd)}")
    print(f"Output directory: {output_dir}")
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=TIMEOUT
        )
        
        # Save output for debugging
        with open(f'{output_dir}/output.log', 'w') as f:
            f.write(result.stdout)
            f.write(result.stderr)
        
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        print(f"Timeout after {TIMEOUT} seconds")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False


def compute_frame_hash(ppm_path):
    """Compute SHA256 hash of PPM frame file."""
    sha256 = hashlib.sha256()
    with open(ppm_path, 'rb') as f:
        # Skip PPM header (first line + dimensions + max value)
        header_lines = 0
        header_data = b''
        for line in f:
            header_data += line
            header_lines += 1
            if header_lines >= 3:
                break
        
        # Hash the pixel data
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            sha256.update(chunk)
    
    return sha256.hexdigest()[:16]  # Short hash for display


def analyze_frame_colors(ppm_path):
    """Analyze RGB color distribution in frame."""
    colors = Counter()
    total_pixels = 0
    
    with open(ppm_path, 'rb') as f:
        # Skip PPM header
        header_lines = 0
        for line in f:
            header_lines += 1
            if header_lines >= 3:
                break
        
        # Read pixel data (RGB triplets)
        while True:
            pixel = f.read(3)
            if len(pixel) < 3:
                break
            
            r, g, b = pixel
            # Quantize to reduce color space
            r_q = (r >> 4) << 4
            g_q = (g >> 4) << 4
            b_q = (b >> 4) << 4
            
            colors[(r_q, g_q, b_q)] += 1
            total_pixels += 1
    
    return colors, total_pixels


def compare_frames(interp_dir, ref_dir, max_frames=MAX_FRAMES):
    """Compare frames from interpreter and reference emulator."""
    print("\n" + "="*60)
    print("FRAME COMPARISON")
    print("="*60)
    
    hash_matches = 0
    hash_mismatches = 0
    total_compared = 0
    
    all_interp_colors = Counter()
    all_ref_colors = Counter()
    total_interp_pixels = 0
    total_ref_pixels = 0
    
    for i in range(max_frames):
        interp_path = f'{interp_dir}/frame_{i:05d}.ppm'
        ref_path = f'{ref_dir}/frame_{i:05d}.ppm'
        
        if not os.path.exists(interp_path) or not os.path.exists(ref_path):
            if i == 0:
                print(f"ERROR: No frames captured!")
                return False
            break
        
        # Compute hashes
        interp_hash = compute_frame_hash(interp_path)
        ref_hash = compute_frame_hash(ref_path)
        
        match = "✓" if interp_hash == ref_hash else "✗"
        print(f"Frame {i:3d}: {match} Interpreter={interp_hash} Reference={ref_hash}")
        
        if interp_hash == ref_hash:
            hash_matches += 1
        else:
            hash_mismatches += 1
        
        total_compared += 1
        
        # Analyze colors
        interp_colors, interp_pixels = analyze_frame_colors(interp_path)
        ref_colors, ref_pixels = analyze_frame_colors(ref_path)
        
        all_interp_colors.update(interp_colors)
        all_ref_colors.update(ref_colors)
        total_interp_pixels += interp_pixels
        total_ref_pixels += ref_pixels
    
    # Summary
    print("\n" + "="*60)
    print("HASH COMPARISON SUMMARY")
    print("="*60)
    print(f"Frames compared: {total_compared}")
    print(f"Hash matches:    {hash_matches} ({100*hash_matches/total_compared:.1f}%)")
    print(f"Hash mismatches: {hash_mismatches} ({100*hash_mismatches/total_compared:.1f}%)")
    
    # Color distribution comparison
    print("\n" + "="*60)
    print("RGB COLOR DISTRIBUTION")
    print("="*60)
    
    print(f"\nInterpreter: {total_interp_pixels} pixels, {len(all_interp_colors)} unique colors")
    print(f"Reference:   {total_ref_pixels} pixels, {len(all_ref_colors)} unique colors")
    
    # Top 10 colors
    print("\nTop 10 colors (Interpreter):")
    for color, count in all_interp_colors.most_common(10):
        r, g, b = color
        pct = 100 * count / total_interp_pixels
        print(f"  RGB({r:3d},{g:3d},{b:3d}): {count:6d} pixels ({pct:5.1f}%)")
    
    print("\nTop 10 colors (Reference):")
    for color, count in all_ref_colors.most_common(10):
        r, g, b = color
        pct = 100 * count / total_ref_pixels
        print(f"  RGB({r:3d},{g:3d},{b:3d}): {count:6d} pixels ({pct:5.1f}%)")
    
    # Color overlap
    common_colors = set(all_interp_colors.keys()) & set(all_ref_colors.keys())
    print(f"\nCommon colors: {len(common_colors)}")
    print(f"Interpreter only: {len(all_interp_colors) - len(common_colors)}")
    print(f"Reference only: {len(all_ref_colors) - len(common_colors)}")
    
    return hash_matches == total_compared


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 compare_frames.py <rom_file>")
        sys.exit(1)
    
    rom_path = sys.argv[1]
    
    if not os.path.exists(rom_path):
        print(f"ERROR: ROM not found: {rom_path}")
        sys.exit(1)
    
    # Paths
    workspace = Path(__file__).parent.parent
    interp_exe = workspace / 'output' / 'a_interp_test' / 'build' / 'a'
    ref_exe = Path.home() / 'Ohjelmat' / 'NES' / 'build' / 'nes'
    
    interp_dir = workspace / 'logs' / 'interp_frames'
    ref_dir = workspace / 'logs' / 'ref_frames'
    
    # Check executables
    if not interp_exe.exists():
        print(f"ERROR: Interpreter executable not found: {interp_exe}")
        print("Run: ./build/bin/nesrecomp roms/a.nes -o output/a_interp_test")
        sys.exit(1)
    
    if not ref_exe.exists():
        print(f"ERROR: Reference emulator not found: {ref_exe}")
        sys.exit(1)
    
    # Clean old frames
    import shutil
    for d in [interp_dir, ref_dir]:
        if d.exists():
            shutil.rmtree(d)
    
    # Run interpreter
    print("\n" + "="*60)
    print("STEP 1: Running interpreter")
    print("="*60)
    if not run_and_capture_frames(
        str(interp_exe), rom_path, str(interp_dir),
        extra_args=['--interpreter', '--limit', str(MAX_FRAMES * 100000)]
    ):
        print("ERROR: Interpreter failed")
        sys.exit(1)
    
    # Run reference emulator
    print("\n" + "="*60)
    print("STEP 2: Running reference emulator")
    print("="*60)
    if not run_and_capture_frames(
        str(ref_exe), rom_path, str(ref_dir),
        extra_args=[]  # Reference emulator doesn't need special args
    ):
        print("ERROR: Reference emulator failed")
        sys.exit(1)
    
    # Compare
    match = compare_frames(str(interp_dir), str(ref_dir))
    
    print("\n" + "="*60)
    if match:
        print("✓ FRAMES MATCH!")
    else:
        print("✗ FRAMES DO NOT MATCH")
    print("="*60)
    
    sys.exit(0 if match else 1)


if __name__ == '__main__':
    main()
