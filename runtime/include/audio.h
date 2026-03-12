/**
 * @file audio.h
 * @brief NES Audio Processing Unit definitions
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "nesrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Public Interface
 * ========================================================================== */

/**
 * @brief Create audio subsystem state
 */
void* nes_audio_create(void);

/**
 * @brief Destroy audio subsystem state
 */
void nes_audio_destroy(void* apu);

/**
 * @brief Reset audio state
 */
void nes_audio_reset(void* apu);

/**
 * @brief Read from audio register
 */
uint8_t nes_audio_read(NESContext* ctx, uint16_t addr);

/**
 * @brief Write to audio register
 */
void nes_audio_write(NESContext* ctx, uint16_t addr, uint8_t value);

/**
 * @brief Step audio subsystem
 */
void nes_audio_step(NESContext* ctx, uint32_t cycles);

/**
 * @brief Advance frame-sequencer timing from DIV transitions
 * @param old_div Divider value before the CPU step
 * @param new_div Divider value after the CPU step
 */
void nes_audio_div_tick(void* apu, uint16_t old_div, uint16_t new_div);

/**
 * @brief Handle a write to DIV, including any APU clock edge it causes
 * @param old_div Divider value before it was reset to 0
 */
void nes_audio_div_reset(void* apu, uint16_t old_div);

/**
 * @brief Get current sample for left/right channels
 * @param apu Audio state
 * @param left Pointer to store left sample
 * @param right Pointer to store right sample
 */
void nes_audio_get_samples(void* apu, int16_t* left, int16_t* right);

/**
 * @brief Enable/disable audio debug capture
 * @param enabled If true, capture audio to debug_audio.raw
 */
void nes_audio_set_debug(bool enabled);

/**
 * @brief Configure debug capture length
 * @param seconds Number of seconds to capture when debug audio is enabled
 */
void nes_audio_set_debug_capture_seconds(uint32_t seconds);

/**
 * @brief Enable/disable audio text tracing
 * @param enabled If true, write APU activity to debug_audio_trace.log
 */
void nes_audio_set_debug_trace(bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
