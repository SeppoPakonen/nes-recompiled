#!/usr/bin/env python3
"""
Trace-Guided Recompilation Tool for NES Recompiler

This script performs iterative trace-guided analysis to discover
all reachable code paths in NES ROMs, especially useful for complex
games with dynamic code paths (MMC1, MMC3, etc.)

Usage:
    python3 trace_guided.py <rom_file> [output_dir] [max_iterations]

Example:
    python3 trace_guided.py roms/a.nes output/a_trace 5
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path
from typing import Optional, Set, Tuple

# Configuration
RECOMPILER = "./build/bin/nesrecomp"
MAX_INSTRUCTIONS = 1000000
RUN_TIMEOUT = 30  # seconds
RUN_FRAMES = 300

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'  # No Color

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

def count_unique_entries(trace_file: str) -> int:
    """Count unique entry points in trace file."""
    if not os.path.exists(trace_file):
        return 0
    
    entries = set()
    with open(trace_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                entries.add(line)
    return len(entries)

def merge_trace_files(old_trace: str, new_trace: str, merged: str):
    """Merge two trace files, keeping unique entries."""
    entries = set()
    
    if os.path.exists(old_trace):
        with open(old_trace, 'r') as f:
            for line in f:
                line = line.strip()
                if line:
                    entries.add(line)
    
    if os.path.exists(new_trace):
        with open(new_trace, 'r') as f:
            for line in f:
                line = line.strip()
                if line:
                    entries.add(line)
    
    with open(merged, 'w') as f:
        for entry in sorted(entries):
            f.write(entry + '\n')
    
    return len(entries)

def run_command(cmd: list, capture: bool = False, check: bool = True) -> Tuple[int, str]:
    """Run a command and return exit code and output."""
    try:
        result = subprocess.run(
            cmd,
            capture_output=capture,
            text=True,
            timeout=RUN_TIMEOUT if not capture else 300
        )
        output = result.stdout + result.stderr if capture else result.stderr
        return result.returncode, output
    except subprocess.TimeoutExpired:
        return -1, "Command timed out"
    except Exception as e:
        return -1, str(e)

def recompile_rom(rom_file: str, output_dir: str, trace_file: Optional[str] = None) -> bool:
    """Recompile ROM with optional trace file."""
    cmd = [RECOMPILER, rom_file, "-o", output_dir, "--verbose"]
    
    if trace_file and os.path.exists(trace_file):
        cmd.extend(["--use-trace", trace_file])
        log_info(f"Using trace file: {trace_file}")
    
    log_step(f"Recompiling ROM to {output_dir}")
    
    returncode, output = run_command(cmd, capture=True)
    
    # Print summary lines
    for line in output.split('\n'):
        if any(x in line for x in ['functions', 'IR blocks', 'Loaded', 'Analyzing']):
            print(f"  {line.strip()}")
    
    return returncode == 0

def build_project(output_dir: str, rom_name: str) -> bool:
    """Build the generated C project."""
    build_dir = os.path.join(output_dir, "build")
    executable = os.path.join(build_dir, rom_name)
    
    log_step(f"Building project in {build_dir}")
    
    # Configure (suppress output)
    returncode, _ = run_command([
        "cmake", "-G", "Ninja",
        "-S", output_dir,
        "-B", build_dir
    ])
    
    if returncode != 0:
        log_error("CMake configuration failed")
        return False
    
    # Build
    returncode, output = run_command(["ninja", "-C", build_dir], capture=True)
    
    # Check if executable exists
    if not os.path.exists(executable):
        log_error(f"Build failed - executable not found: {executable}")
        return False
    
    log_success("Build successful")
    return True

def run_with_trace(executable: str, trace_file: str, instructions: int = MAX_INSTRUCTIONS) -> bool:
    """Run executable with trace enabled."""
    log_step(f"Running ROM with trace (limit: {instructions} instructions)")
    
    cmd = [
        executable,
        "--trace-entries", trace_file,
        "--limit", str(instructions)
    ]
    
    returncode, output = run_command(cmd, capture=True)
    
    # Print last few lines
    lines = output.strip().split('\n')
    if len(lines) > 5:
        for line in lines[-5:]:
            if line.strip():
                print(f"  {line.strip()}")
    
    return os.path.exists(trace_file)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("Arguments:")
        print("  rom_file       Path to the NES ROM file")
        print("  output_dir     Base directory for output (default: output/trace_guided)")
        print("  max_iterations Maximum iterations (default: 10)")
        sys.exit(1)
    
    rom_file = sys.argv[1]
    output_base = sys.argv[2] if len(sys.argv) > 2 else "output/trace_guided"
    max_iterations = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    
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
    
    trace_file = os.path.join(logs_dir, f"{rom_name}_trace.log")
    merged_trace = os.path.join(logs_dir, f"{rom_name}_merged_trace.log")
    
    # Print header
    print(f"\n{Colors.BLUE}{'='*60}{Colors.NC}")
    print(f"{Colors.BLUE}Trace-Guided Recompilation Tool{Colors.NC}")
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}\n")
    print(f"ROM: {rom_file}")
    print(f"Output base: {output_base}")
    print(f"Max iterations: {max_iterations}")
    print(f"Max instructions per run: {MAX_INSTRUCTIONS}")
    print()
    
    # Check for existing trace
    prev_entries = count_unique_entries(trace_file)
    if prev_entries > 0:
        log_info(f"Found existing trace file with {prev_entries} unique entries")
        merge_trace_files("", trace_file, merged_trace)
    
    # Main iteration loop
    iteration = 0
    prev_entries = count_unique_entries(merged_trace)
    
    while iteration < max_iterations:
        iteration += 1
        output_dir = f"{output_base}_v{iteration}"
        iter_trace = os.path.join(logs_dir, f"{rom_name}_iter_{iteration}.log")
        
        print(f"\n{Colors.BLUE}{'='*60}{Colors.NC}")
        print(f"{Colors.BLUE}Iteration {iteration} / {max_iterations}{Colors.NC}")
        print(f"{Colors.BLUE}{'='*60}{Colors.NC}")
        
        # Step 1: Recompile
        trace_to_use = merged_trace if os.path.exists(merged_trace) else None
        if not recompile_rom(rom_file, output_dir, trace_to_use):
            log_error("Recompilation failed")
            sys.exit(1)

        # Step 2: Build
        if not build_project(output_dir, rom_name):
            log_error("Build failed")
            sys.exit(1)
        
        # Step 3: Run with trace
        executable = os.path.join(output_dir, "build", rom_name)
        if not run_with_trace(executable, iter_trace):
            log_error("Failed to create trace file")
            sys.exit(1)
        
        # Step 4: Merge traces
        log_step("Merging trace files")
        total_entries = merge_trace_files(merged_trace, iter_trace, merged_trace + ".new")
        os.rename(merged_trace + ".new", merged_trace)
        
        # Also update main trace file
        shutil.copy(merged_trace, trace_file)
        
        # Step 5: Check for convergence
        new_entries = total_entries - prev_entries
        
        print()
        log_info(f"Trace entries this iteration: {count_unique_entries(iter_trace)}")
        log_info(f"Total unique entries: {total_entries}")
        
        if new_entries > 0:
            log_success(f"Found {new_entries} new entry points")
            prev_entries = total_entries
        else:
            log_success("No new entry points found - analysis complete!")
            break
    
    # Summary
    print(f"\n{Colors.BLUE}{'='*60}{Colors.NC}")
    print(f"{Colors.BLUE}Analysis Complete{Colors.NC}")
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}\n")
    
    print("Results:")
    print(f"  Iterations: {iteration}")
    print(f"  Total unique entry points: {count_unique_entries(merged_trace)}")
    print(f"  Final output: {output_dir}")
    print(f"  Trace file: {trace_file}")
    print(f"  Merged trace: {merged_trace}")
    print()
    print("To run the final recompiled ROM:")
    print(f"  {output_dir}/build/{rom_name}")
    print()
    print("To recompile with the collected trace:")
    print(f"  {RECOMPILER} {rom_file} -o output/final --use-trace {merged_trace}")
    print()

if __name__ == "__main__":
    main()
