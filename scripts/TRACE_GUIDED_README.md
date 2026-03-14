# Trace-Guided Analysis for NES Recompiler

## Overview

Trace-guided analysis uses runtime execution traces to discover code paths that static analysis misses. This is especially useful for complex NES games with:

- **Dynamic code paths** - Code that's only executed under certain conditions
- **Bank-switched code** - Different ROM banks mapped at runtime
- **Computed jumps** - Jump tables and indirect jumps
- **Runtime-loaded code** - Code loaded from CHR ROM or other sources

## How It Works

1. **Initial Analysis**: The recompiler performs static analysis to find code
2. **Instrumentation**: Generated code logs all executed addresses to a trace file
3. **Execution**: Run the game to collect runtime entry points
4. **Re-analysis**: Recompile using discovered entry points
5. **Iteration**: Repeat until no new entry points are found

## Usage

### Python Script (Recommended)

```bash
# Basic usage
python3 scripts/trace_guided.py roms/a.nes

# With custom output directory
python3 scripts/trace_guided.py roms/a.nes output/a_trace

# Limit iterations
python3 scripts/trace_guided.py roms/a.nes output/a_trace 5
```

### Bash Script

```bash
# Basic usage
./scripts/trace_guided_recomp.sh roms/a.nes

# With custom parameters
./scripts/trace_guided_recomp.sh roms/a.nes output/a_trace 5
```

### Manual Process

```bash
# Step 1: Initial recompile
./build/bin/nesrecomp roms/game.nes -o output/game_v1

# Step 2: Build
cmake -G Ninja -S output/game_v1 -B output/game_v1/build
ninja -C output/game_v1/build

# Step 3: Run with trace
./output/game_v1/build/game --trace-entries logs/game_trace.log --limit 1000000

# Step 4: Recompile with trace
./build/bin/nesrecomp roms/game.nes -o output/game_v2 --use-trace logs/game_trace.log

# Step 5: Repeat steps 2-4 until no new entry points
```

## Command Line Options

The generated executables support these trace-related options:

| Option | Description |
|--------|-------------|
| `--trace-entries <file>` | Write executed entry points to file |
| `--limit <N>` | Stop after N instructions (prevent infinite loops) |

The recompiler supports:

| Option | Description |
|--------|-------------|
| `--use-trace <file>` | Use trace file to discover additional entry points |
| `--verbose` | Show detailed analysis statistics |

## Trace File Format

Trace files use a simple format: `bank:address` (hex)

```
0:C000
0:C014
0:C019
0:C035
7:C000
7:C035
```

## Example Output

```
============================================================
Trace-Guided Recompilation Tool
============================================================

ROM: roms/a.nes
Output base: output/trace_guided
Max iterations: 10
Max instructions per run: 1000000

[INFO] Found existing trace file with 9 unique entries

============================================================
Iteration 1 / 10
============================================================

==> Recompiling ROM to output/trace_guided_v1
  Using trace file: logs/a_merged_trace.log
  Loaded 9 entry points from trace file
  Found 1187 functions
  9008 IR blocks

==> Building project in output/trace_guided_v1/build
[SUCCESS] Build successful

==> Running ROM with trace (limit: 1000000 instructions)

==> Merging trace files
[INFO] Trace entries this iteration: 249869
[INFO] Total unique entries: 9
[SUCCESS] No new entry points found - analysis complete!

============================================================
Analysis Complete
============================================================

Results:
  Iterations: 1
  Total unique entry points: 9
  Final output: output/trace_guided_v1
  Trace file: logs/a_trace.log
  Merged trace: logs/a_merged_trace.log
```

## Troubleshooting

### Game stuck in infinite loop

Use `--limit` to prevent infinite execution:

```bash
./output/game/build/game --trace-entries logs/trace.log --limit 1000000
```

### No new entry points found

This can happen if:
1. The game is stuck in a loop (check logs)
2. All reachable code has been found
3. The game needs user input to progress

### Out of memory

Reduce `MAX_INSTRUCTIONS` in the script or use `--limit`:

```bash
python3 scripts/trace_guided.py roms/large_game.nes output/large 3
```

## Best Practices

1. **Start with small instruction limits** (100k-1M) to detect loops
2. **Watch the logs** for repeated patterns indicating stuck code
3. **Use meaningful iteration limits** (5-10 is usually enough)
4. **Keep trace files** - they're useful for debugging
5. **Check generated C code** if analysis seems incomplete

## Advanced: Custom Trace Collection

For games that need specific input sequences:

```bash
# Run with custom input simulation
./output/game/build/game --trace-entries logs/trace.log --limit 1000000

# Or modify the generated main.c to simulate input
```

## Files Generated

| File | Description |
|------|-------------|
| `logs/<rom>_trace.log` | Current iteration trace |
| `logs/<rom>_merged_trace.log` | All discovered entry points |
| `logs/<rom>_iter_N.log` | Per-iteration traces |
| `output/<rom>_vN/` | Generated C code per iteration |

## See Also

- [QWEN.md](../QWEN.md) - Project documentation
- [plan/cookie.txt](../plan/cookie.txt) - Current progress and known issues
