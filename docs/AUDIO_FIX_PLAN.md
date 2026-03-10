# Audio Performance Fix Plan for GB Recompiled

## Problem Summary

When running Pokemon Blue (and other complex games), audio is choppy during real-time playback, but the exported WAV file (`debug_audio.raw` → `debug_audio.wav`) sounds perfect. This indicates the audio **generation** is correct, but the **playback pipeline** has issues.

## Root Causes Identified

### 1. CRITICAL: Debug Audio Logging Enabled (file I/O every sample)
**Location:** `runtime/src/audio.c:106`
```c
#define AUDIO_DEBUG_LOGGING
```

**Impact:** `fwrite()` is called for EVERY audio sample (~44,100 times/second) for the first 10 seconds. File I/O is extremely slow and blocks the main thread, causing frame stuttering and audio underruns.

**Fix:** Make debug logging conditional with a runtime flag, not a compile-time define.

---

### 2. HIGH: Audio Samples Dropped When Queue Is "Full"
**Location:** `runtime/src/platform_sdl.cpp:183-186`
```cpp
if (g_audio_batch_pos >= AUDIO_BATCH_SIZE) {
    if (SDL_GetQueuedAudioSize(g_audio_device) < 16384) {  // <-- PROBLEM
        SDL_QueueAudio(g_audio_device, g_audio_batch, sizeof(g_audio_batch));
    }
    g_audio_batch_pos = 0;  // Reset even if we didn't queue!
}
```

**Impact:** When the SDL audio queue has >16KB buffered (~46ms), the entire batch is **silently discarded**. The batch position resets to 0, losing 1024 samples (~23ms) of audio.

**Fix:** Always queue the audio. Let `gb_platform_vsync()` handle backpressure by waiting when the queue is too full.

---

### 3. MEDIUM: Sample Rate Drift (Integer vs Float Timing)
**Location:** `runtime/src/audio.c:168`
```c
apu->sample_period = 95;  // Should be 95.1
```

**Impact:** 
- Actual: 4194304 Hz / 44100 Hz = **95.108** cycles per sample
- Used: **95** cycles per sample
- Result: Audio runs ~0.1% too fast, causing 1 extra sample every ~1000 samples
- Over 10 seconds: ~44 extra samples generated, eventually causing buffer overflow

**Fix:** Use fixed-point arithmetic or a fractional accumulator.

---

### 4. MEDIUM: Channel 4 (Noise) LFSR Can Spin Excessively
**Location:** `runtime/src/audio.c:493-517`
```c
while (apu->ch4.accum >= period) {
    apu->ch4.accum -= period;
    // LFSR shifting...
}
```

**Impact:** When noise frequency is very high (low divisor) and `cycles` is large, this loop can iterate thousands of times per call, causing CPU spikes.

**Fix:** Pre-calculate iteration count: `iterations = accum / period`, then use bit manipulation for LFSR advance.

---

### 5. LOW: VSync Threshold Mismatch
**Location:** `runtime/src/platform_sdl.cpp:561-579`

The vsync wait threshold (8192 bytes) and queue drop threshold (16384 bytes) create an unstable window where audio may be dropped without vsync compensation.

---

## Implementation Plan

### Phase 1: Quick Fixes (Immediate Performance Improvement)

1. **Disable debug audio logging by default**
   - Add runtime flag `--debug-audio` to enable
   - Remove `#define AUDIO_DEBUG_LOGGING`

2. **Remove queue size check for batching**
   ```cpp
   if (g_audio_batch_pos >= AUDIO_BATCH_SIZE) {
       SDL_QueueAudio(g_audio_device, g_audio_batch, sizeof(g_audio_batch));
       g_audio_batch_pos = 0;
   }
   ```

3. **Increase audio buffer sizes for more latency tolerance**
   - Increase `want.samples` from 1024 to 2048
   - Adjust vsync thresholds accordingly

### Phase 2: Accuracy Fixes (Correct Timing)

1. **Use fixed-point sample timing**
   ```c
   #define SAMPLE_PERIOD_FIXED (uint32_t)(95.108 * 256)  // 24358 in 8.8 fixed point
   
   apu->sample_timer_fixed += cycles << 8;
   while (apu->sample_timer_fixed >= SAMPLE_PERIOD_FIXED) {
       apu->sample_timer_fixed -= SAMPLE_PERIOD_FIXED;
       // Generate sample
   }
   ```

2. **Optimize Channel 4 LFSR stepping**
   - Pre-calculate number of shifts needed
   - For 7-bit mode, use lookup tables

### Phase 3: Debugging Tools

1. **Audio statistics overlay in ImGui**
   - Show queue size, samples generated, samples dropped
   - Show per-frame audio generation time

2. **Runtime audio diagnostics**
   - Use `tools/diagnose_audio.py` to analyze captured audio
   - Add `--audio-stats` flag to print queue health every second

---

## Files to Modify

| File | Changes |
|------|---------|
| `runtime/src/audio.c` | Remove compile-time debug logging, add runtime flag, fix sample timing |
| `runtime/src/platform_sdl.cpp` | Remove queue size check, adjust buffer sizes, add stats tracking |
| `runtime/include/audio.h` | Add audio stats structure for debugging |
| `runtime/include/gbrt.h` | Add `gbrt_audio_debug` flag |

---

## Testing

1. Run Pokemon Blue for 60 seconds
2. Check for audio dropouts using `diagnose_audio.py`
3. Verify FPS stays at 60 with no major drops
4. Compare CPU usage before/after fixes

---

## Commands

```bash
# Build with fixes
ninja -C build

# Run with audio debugging
./build/bin/gbrecomp roms/pokeblue.gb -o output/pokeblue_audio_test
cmake -G Ninja -S output/pokeblue_audio_test -B output/pokeblue_audio_test/build
ninja -C output/pokeblue_audio_test/build

# Run and analyze
./output/pokeblue_audio_test/build/pokeblue_audio_test
python3 tools/diagnose_audio.py
python3 tools/analyze_audio.py
```
