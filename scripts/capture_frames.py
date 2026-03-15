#!/usr/bin/env python3
"""
Simple frame capture and analysis for NES interpreter testing.

Captures frames from the interpreter and analyzes RGB distribution.
Compares against expected patterns.

Usage:
    python3 scripts/capture_frames.py roms/a.nes --interpreter
"""

import subprocess
import os
import sys
import hashlib
from pathlib import Path
from collections import Counter

def run_interpreter(rom_path, output_dir, max_frames=100):
    """Run interpreter and capture frames."""
    output_dir = Path(output_dir)
    os.makedirs(output_dir, exist_ok=True)
    
    # Build the interpreter executable
    workspace = Path(__file__).parent.parent
    interp_exe = workspace / 'output' / 'a_interp_test' / 'build' / 'a'
    
    if not interp_exe.exists():
        print(f"ERROR: Executable not found: {interp_exe}")
        print("Run: ./build/bin/nesrecomp roms/a.nes -o output/a_interp_test")
        print("     cmake -G Ninja -S output/a_interp_test -B output/a_interp_test/build")
        print("     ninja -C output/a_interp_test/build")
        return False
    
    # Use absolute path for screenshot prefix to ensure files are saved in output_dir
    screenshot_prefix = str(output_dir / 'frame')
    
    cmd = [
        str(interp_exe),
        rom_path,
        '--interpreter',
        '--limit', str(max_frames * 100000),
        '--auto-screenshot',
        '--screenshot-prefix', screenshot_prefix
    ]
    
    print(f"Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=60
        )
        
        # Save output
        with open(f'{output_dir}/output.log', 'w') as f:
            f.write(result.stdout)
            f.write(result.stderr)
        
        print(f"Exit code: {result.returncode}")
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        print("Timeout!")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False


def analyze_ppm(ppm_path):
    """Analyze a PPM frame file."""
    if not os.path.exists(ppm_path):
        return None
    
    colors = Counter()
    total_pixels = 0
    
    with open(ppm_path, 'rb') as f:
        # Read header
        magic = f.readline().strip()
        if magic != b'P6':
            print(f"  Invalid PPM format: {ppm_path}")
            return None
        
        # Read dimensions
        dimensions = f.readline().strip().decode()
        width, height = map(int, dimensions.split())
        
        # Read max value
        max_val = int(f.readline().strip())
        
        # Read pixel data
        while True:
            pixel = f.read(3)
            if len(pixel) < 3:
                break
            
            r, g, b = pixel
            # Categorize by brightness
            brightness = (r + g + b) // 3
            
            if brightness < 64:
                category = 'dark'
            elif brightness < 128:
                category = 'medium'
            elif brightness < 192:
                category = 'bright'
            else:
                category = 'very_bright'
            
            colors[category] += 1
            
            # Also track exact colors
            r_q = (r >> 4) << 4
            g_q = (g >> 4) << 4
            b_q = (b >> 4) << 4
            colors[(r_q, g_q, b_q)] += 1
            
            total_pixels += 1
    
    return {
        'width': width,
        'height': height,
        'total_pixels': total_pixels,
        'colors': colors,
        'unique_colors': len([k for k in colors if isinstance(k, tuple)])
    }


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 capture_frames.py <rom_file> [--interpreter]")
        sys.exit(1)
    
    rom_path = sys.argv[1]
    use_interpreter = '--interpreter' in sys.argv
    
    if not os.path.exists(rom_path):
        print(f"ERROR: ROM not found: {rom_path}")
        sys.exit(1)
    
    workspace = Path(__file__).parent.parent
    output_dir = workspace / 'logs' / 'frame_capture'

    # Check if frames already exist
    existing_frames = list(output_dir.glob('frame_*.ppm'))
    
    # Run interpreter only if no existing frames
    if not existing_frames:
        print("\n" + "="*60)
        print("CAPTURING FRAMES")
        print("="*60)

        if not run_interpreter(str(rom_path), str(output_dir), max_frames=10):
            print("ERROR: Failed to capture frames")
            sys.exit(1)
    else:
        print(f"\nUsing {len(existing_frames)} existing frames in {output_dir}")
    
    # Analyze frames
    print("\n" + "="*60)
    print("FRAME ANALYSIS")
    print("="*60)

    frames_analyzed = 0
    all_colors = Counter()
    total_pixels = 0

    for i in range(1, 101):  # Start from frame 1
        ppm_path = output_dir / f'frame_{i:05d}.ppm'
        if not ppm_path.exists():
            if frames_analyzed == 0:
                print("ERROR: No frames captured!")
                sys.exit(1)
            break

        analysis = analyze_ppm(str(ppm_path))
        if analysis:
            frames_analyzed += 1
            all_colors.update(analysis['colors'])
            total_pixels += analysis['total_pixels']
            
            # Print frame summary
            dark = analysis['colors'].get('dark', 0)
            medium = analysis['colors'].get('medium', 0)
            bright = analysis['colors'].get('bright', 0)
            very_bright = analysis['colors'].get('very_bright', 0)
            
            print(f"Frame {i:3d}: {analysis['width']}x{analysis['height']} "
                  f"Dark={dark:5d} Med={medium:5d} Bright={bright:5d} "
                  f"VBright={very_bright:5d} Colors={analysis['unique_colors']:4d}")
    
    # Summary
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    print(f"Frames analyzed: {frames_analyzed}")
    print(f"Total pixels: {total_pixels}")
    print(f"Unique colors: {len([k for k in all_colors if isinstance(k, tuple)])}")
    
    # Top colors
    print("\nTop 10 colors:")
    for color, count in all_colors.most_common(10):
        if isinstance(color, tuple):
            r, g, b = color
            pct = 100 * count / total_pixels
            brightness = (r + g + b) // 3
            print(f"  RGB({r:3d},{g:3d},{b:3d}): {count:6d} ({pct:5.1f}%) - brightness={brightness:3d}")
    
    # Brightness distribution
    dark = all_colors.get('dark', 0)
    medium = all_colors.get('medium', 0)
    bright = all_colors.get('bright', 0)
    very_bright = all_colors.get('very_bright', 0)
    
    print(f"\nBrightness distribution:")
    print(f"  Dark (<64):       {dark:6d} ({100*dark/total_pixels:5.1f}%)")
    print(f"  Medium (64-127):  {medium:6d} ({100*medium/total_pixels:5.1f}%)")
    print(f"  Bright (128-191): {bright:6d} ({100*bright/total_pixels:5.1f}%)")
    print(f"  Very Bright (>192): {very_bright:6d} ({100*very_bright/total_pixels:5.1f}%)")
    
    # Check for solid grayscale (bug indicator)
    if len([k for k in all_colors if isinstance(k, tuple)]) < 10:
        print("\n⚠️  WARNING: Very few unique colors - possible solid grayscale bug!")
    
    if dark > total_pixels * 0.9:
        print("\n⚠️  WARNING: >90% dark pixels - possible black screen bug!")
    
    if very_bright > total_pixels * 0.9:
        print("\n⚠️  WARNING: >90% very bright pixels - possible white/gray screen bug!")


if __name__ == '__main__':
    main()
