#!/usr/bin/env python3
"""
Emulator Trace Extraction Tool

This tool runs a ROM using the recompiled interpreter to capture the actual
execution trace, then uses that trace for guided recompilation.

The interpreter logs every executed address, giving us accurate code paths
that the game actually uses during execution.

Usage:
    python3 emulator_trace.py roms/a.nes output/a_emu 1000000
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path
from collections import Counter
from typing import Optional, Tuple

# Configuration
RECOMPILER = "./build/bin/nesrecomp"
DEFAULT_INSTRUCTIONS = 1000000
RUN_TIMEOUT = 60  # seconds

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'

def log_info(msg: str):
    print(f"{Colors.BLUE}[INFO]{Colors.NC} {msg}")

def log_success(msg: str):
    print(f"{Colors.GREEN}[SUCCESS]{Colors.NC} {msg}")

def log_warning(msg: str):
    print(f"{Colors.YELLOW}[WARNING]{Colors.NC} {msg}")

def log_error(msg: str):
    print(f"{Colors.RED}[ERROR]{Colors.NC} {msg}")

def log_step(msg: str):
    print(f"\n{Colors.CYAN}==>{Colors.NC} {msg}")

def run_command(cmd: list, capture: bool = False, timeout: int = RUN_TIMEOUT) -> Tuple[int, str]:
    """Run a command and return exit code and output."""
    try:
        result = subprocess.run(
            cmd,
            capture_output=capture,
            text=True,
            timeout=timeout
        )
        output = result.stdout + result.stderr if capture else result.stderr
        return result.returncode, output
    except subprocess.TimeoutExpired:
        return -1, "Command timed out"
    except Exception as e:
        return -1, str(e)

def analyze_trace(trace_file: str) -> dict:
    """Analyze trace file and return statistics."""
    if not os.path.exists(trace_file):
        return {}
    
    entries = []
    with open(trace_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line and ':' in line:
                entries.append(line)
    
    if not entries:
        return {}
    
    # Count frequency
    counter = Counter(entries)
    
    # Parse bank:address
    banks = {}
    for entry in counter:
        bank, addr = entry.split(':')
        if bank not in banks:
            banks[bank] = []
        banks[bank].append((int(addr, 16), counter[entry]))
    
    return {
        'total': len(entries),
        'unique': len(counter),
        'top_20': counter.most_common(20),
        'banks': banks
    }

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("Arguments:")
        print("  rom_file       Path to the NES ROM file")
        print("  output_dir     Base directory for output (default: output/emulator_trace)")
        print("  instructions   Max instructions to trace (default: 1000000)")
        sys.exit(1)
    
    rom_file = sys.argv[1]
    output_base = sys.argv[2] if len(sys.argv) > 2 else "output/emulator_trace"
    max_instructions = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_INSTRUCTIONS
    
    if not os.path.exists(rom_file):
        log_error(f"ROM file not found: {rom_file}")
        sys.exit(1)
    
    if not os.path.exists(RECOMPILER):
        log_error(f"Recompiler not found: {RECOMPILER}")
        log_error("Run 'ninja -C build' first")
        sys.exit(1)
    
    # Setup paths
    rom_name = Path(rom_file).stem
    logs_dir = "logs"
    os.makedirs(logs_dir, exist_ok=True)
    os.makedirs(output_base, exist_ok=True)
    
    trace_file = os.path.join(logs_dir, f"{rom_name}_emu_trace.log")
    merged_trace = os.path.join(logs_dir, f"{rom_name}_merged_trace.log")
    
    # Print header
    print(f"\n{Colors.BLUE}{'='*60}{Colors.NC}")
    print(f"{Colors.BLUE}Emulator Trace Extraction{Colors.NC}")
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}\n")
    print(f"ROM: {rom_file}")
    print(f"Output: {output_base}")
    print(f"Max instructions: {max_instructions}")
    print()
    
    # Step 1: Initial recompile
    log_step("Step 1: Initial recompilation")
    
    initial_dir = f"{output_base}_initial"
    cmd = [RECOMPILER, rom_file, "-o", initial_dir, "--verbose"]
    returncode, output = run_command(cmd, capture=True)
    
    if returncode != 0:
        log_error("Recompilation failed")
        print(output)
        sys.exit(1)
    
    for line in output.split('\n'):
        if any(x in line for x in ['functions', 'IR blocks']):
            print(f"  {line.strip()}")
    
    # Step 2: Build
    log_step("Step 2: Building")
    
    build_dir = os.path.join(initial_dir, "build")
    
    returncode, _ = run_command(["cmake", "-G", "Ninja", "-S", initial_dir, "-B", build_dir])
    if returncode != 0:
        log_error("CMake failed")
        sys.exit(1)
    
    executable = os.path.join(build_dir, rom_name)
    returncode, output = run_command(["ninja", "-C", build_dir], timeout=300)
    
    # Check if executable exists (ninja may return non-zero for warnings)
    if not os.path.exists(executable):
        log_error(f"Ninja build failed - executable not found")
        sys.exit(1)
    
    log_success(f"Build successful: {executable}")
    
    # Step 3: Run with trace
    log_step("Step 3: Running ROM to capture trace")
    print("This will run the ROM and log all executed addresses...")
    print()
    
    cmd = [
        executable,
        "--trace-entries", trace_file,
        "--limit", str(max_instructions)
    ]
    
    returncode, output = run_command(cmd, capture=True)
    
    # Print last lines of output
    lines = output.strip().split('\n')
    if len(lines) > 5:
        for line in lines[-5:]:
            if line.strip():
                print(f"  {line.strip()}")
    
    if not os.path.exists(trace_file):
        log_error("Trace file not created")
        sys.exit(1)
    
    # Analyze trace
    stats = analyze_trace(trace_file)
    
    print()
    log_success("Trace captured successfully!")
    print(f"  Total entries: {stats.get('total', 0):,}")
    print(f"  Unique entry points: {stats.get('unique', 0):,}")
    print()
    
    # Show top entry points
    print(f"{Colors.CYAN}Top 20 most executed addresses:{Colors.NC}")
    for addr, count in stats.get('top_20', []):
        bank, offset = addr.split(':')
        print(f"  {bank}:{offset:>5s}  {count:>10,} times")
    print()
    
    # Show bank distribution
    print(f"{Colors.CYAN}Entry points by bank:{Colors.NC}")
    for bank, entries in sorted(stats.get('banks', {}).items()):
        print(f"  Bank {bank}: {len(entries)} unique addresses")
    print()
    
    # Step 4: Recompile with trace
    log_step("Step 4: Recompiling with trace-guided analysis")
    
    final_dir = f"{output_base}_final"
    cmd = [RECOMPILER, rom_file, "-o", final_dir, "--use-trace", trace_file, "--verbose"]
    returncode, output = run_command(cmd, capture=True)
    
    for line in output.split('\n'):
        if any(x in line for x in ['functions', 'IR blocks', 'Loaded', 'entry']):
            print(f"  {line.strip()}")
    
    if returncode != 0:
        log_warning("Recompilation had issues (this is OK)")
    
    # Step 5: Build final version
    log_step("Step 5: Building final version")
    
    final_build = os.path.join(final_dir, "build")
    
    returncode, _ = run_command(["cmake", "-G", "Ninja", "-S", final_dir, "-B", final_build])
    if returncode != 0:
        log_error("CMake failed")
        sys.exit(1)

    final_exe = os.path.join(final_build, rom_name)
    returncode, _ = run_command(["ninja", "-C", final_build], timeout=300)
    
    if not os.path.exists(final_exe):
        log_error(f"Ninja build failed - executable not found")
        sys.exit(1)

    log_success(f"Build successful: {final_exe}")
    
    # Summary
    print(f"\n{Colors.BLUE}{'='*60}{Colors.NC}")
    print(f"{Colors.BLUE}Trace Extraction Complete{Colors.NC}")
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}\n")
    
    print("Results:")
    print(f"  Trace file: {trace_file}")
    print(f"  Total entries: {stats.get('total', 0):,}")
    print(f"  Unique entry points: {stats.get('unique', 0):,}")
    print(f"  Final output: {final_dir}")
    print()
    print("To run the final recompiled ROM:")
    print(f"  {final_exe}")
    print()
    print("To merge with existing traces:")
    print(f"  cat {trace_file} logs/*_merged_trace.log | sort -u > {merged_trace}")
    print()

if __name__ == "__main__":
    main()
