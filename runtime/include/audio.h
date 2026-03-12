/**
 * @file audio.h
 * @brief NES Audio Processing Unit (APU) definitions
 *
 * Implements the NES APU with:
 * - 2 Pulse channels (square waves with duty cycle)
 * - 1 Triangle channel
 * - 1 Noise channel (LFSR-based)
 * - 1 DMC channel (delta modulation, stub implementation)
 * - Frame counter (mode 0/1)
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "nesrt.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ========================================================================== */

#define NES_APU_SAMPLING_RATE 48000
#define NES_APU_AUDIO_BUFFER_SIZE 2048
#define NES_APU_NTSC_CLOCK 21477272
#define NES_APU_NTSC_FRAME_RATE 60.0988
#define NES_APU_PAL_CLOCK 21281370
#define NES_APU_PAL_FRAME_RATE 50.0

/* Frame sequencer timing (NTSC cycles) */
#define NES_APU_FRAME_STEP_1 7457
#define NES_APU_FRAME_STEP_2 14913
#define NES_APU_FRAME_STEP_3 22371
#define NES_APU_FRAME_STEP_4 29828
#define NES_APU_FRAME_STEP_5 37281

/* Length counter lookup table */
#define NES_APU_LENGTH_COUNTER_SIZE 32

/* Duty cycle patterns (8 steps) */
#define NES_APU_DUTY_PATTERN_SIZE 8

/* Triangle waveform sequence */
#define NES_APU_TRIANGLE_SEQUENCE_SIZE 32

/* Noise period lookup (NTSC) */
#define NES_APU_NOISE_PERIOD_SIZE 16

/* DMC rate lookup (NTSC) */
#define NES_APU_DMC_RATE_SIZE 16

/* LUT sizes for mixer */
#define NES_APU_PULSE_LUT_SIZE 31
#define NES_APU_TND_LUT_SIZE 203

/* ============================================================================
 * APU Channel Structures
 * ========================================================================== */

/**
 * @brief Divider/Timer structure for channel timing
 */
typedef struct {
    uint32_t period;      /**< Timer period */
    uint32_t counter;     /**< Current counter value */
    uint32_t step;        /**< Current sequencer step */
    uint32_t limit;       /**< Maximum step value */
    uint32_t from;        /**< Reset step value */
    uint8_t  loop;        /**< Loop enable */
} NESApuDivider;

/**
 * @brief Pulse channel state (used for channels 1 and 2)
 */
typedef struct {
    NESApuDivider timer;      /**< Waveform timer */
    NESApuDivider envelope;   /**< Envelope timer */
    NESApuDivider sweep;      /**< Sweep timer */
    
    uint8_t  ctrl;            /**< Control register ($4000/$4004) */
    uint8_t  ramp;            /**< Ramp register ($4001/$4005) */
    uint16_t freq;            /**< Frequency ($4002/$4003, $4006/$4007) */
    
    uint8_t  length_counter;  /**< Length counter */
    uint8_t  duty;            /**< Duty cycle (0-3) */
    uint8_t  envelope_volume; /**< Current envelope volume */
    uint8_t  envelope_started;/**< Envelope started flag */
    
    /* Sweep state */
    uint8_t  sweep_enabled;   /**< Sweep enable */
    uint8_t  sweep_neg;       /**< Sweep direction (down/up) */
    uint8_t  sweep_shift;     /**< Sweep shift amount */
    uint8_t  sweep_reload;    /**< Sweep reload flag */
    uint8_t  sweep_mute;      /**< Muted by sweep */
    
    uint8_t  enabled;         /**< Channel enabled */
    uint8_t  id;              /**< Channel ID (1 or 2) */
} NESApuPulse;

/**
 * @brief Triangle channel state
 */
typedef struct {
    NESApuDivider timer;      /**< Waveform timer */
    
    uint8_t  ctrl;            /**< Control register ($4008) */
    uint16_t freq;            /**< Frequency ($400A/$400B) */
    
    uint8_t  length_counter;  /**< Length counter */
    uint8_t  linear_counter;  /**< Linear counter */
    uint8_t  linear_reload;   /**< Linear counter reload value */
    uint8_t  linear_reload_flag; /**< Linear reload pending */
    uint8_t  halt;            /**< Halt flag */
    
    uint8_t  enabled;         /**< Channel enabled */
    uint8_t  sequencer_step;  /**< Current sequencer step (0-31) */
} NESApuTriangle;

/**
 * @brief Noise channel state
 */
typedef struct {
    NESApuDivider timer;      /**< Noise timer */
    NESApuDivider envelope;   /**< Envelope timer */
    
    uint8_t  ctrl;            /**< Control register ($400C) */
    uint8_t  freq;            /**< Frequency index ($400E) */
    
    uint8_t  length_counter;  /**< Length counter */
    uint8_t  envelope_volume; /**< Current envelope volume */
    uint8_t  envelope_started;/**< Envelope started flag */
    
    uint16_t shift_register;  /**< 15-bit LFSR */
    uint8_t  mode;            /**< LFSR mode (0=7-bit, 1=15-bit) */
    
    uint8_t  enabled;         /**< Channel enabled */
} NESApuNoise;

/**
 * @brief DMC (Delta Modulation Channel) state
 */
typedef struct {
    uint8_t  ctrl;            /**< Control register ($4010) */
    uint8_t  da;              /**< Direct/Load counter ($4011) */
    uint8_t  addr;            /**< Sample address ($4012) */
    uint8_t  len;             /**< Sample length ($4013) */
    
    uint16_t rate;            /**< Current rate */
    uint16_t rate_index;      /**< Rate counter */
    uint16_t sample_addr;     /**< Sample start address */
    uint16_t sample_length;   /**< Sample length in bytes */
    
    /* Output unit */
    uint8_t  counter;         /**< Output counter (7-bit) */
    uint8_t  bits_remaining;  /**< Bits remaining in current sample */
    uint8_t  silence;         /**< Silence flag */
    uint8_t  bits;            /**< Current sample bits */
    
    /* Memory reader */
    uint8_t  sample;          /**< Current sample byte */
    uint8_t  empty;           /**< Buffer empty flag */
    uint16_t bytes_remaining; /**< Bytes remaining */
    uint16_t current_addr;    /**< Current read address */
    
    uint8_t  loop;            /**< Loop enable */
    uint8_t  irq_enable;      /**< IRQ enable */
    uint8_t  interrupt;       /**< Interrupt pending */
    uint8_t  irq_set;         /**< IRQ has been set */
    uint8_t  enabled;         /**< Channel enabled */
} NESApuDmc;

/**
 * @brief Frame sequencer state
 */
typedef struct {
    uint8_t  mode;            /**< Frame mode (0=4-step, 1=5-step) */
    uint8_t  step;            /**< Current step (0-4) */
    uint32_t cycles;          /**< Cycle counter */
    uint8_t  irq_pending;     /**< Frame IRQ pending */
    uint8_t  irq_inhibit;     /**< IRQ inhibit flag */
} NESApuFrameSequencer;

/**
 * @brief Main APU state structure
 */
typedef struct {
    /* Channels */
    NESApuPulse pulse1;
    NESApuPulse pulse2;
    NESApuTriangle triangle;
    NESApuNoise noise;
    NESApuDmc dmc;
    
    /* Frame sequencer */
    NESApuFrameSequencer frame;
    
    /* Status register */
    uint8_t status;         /* $4015 - Channel enable/status */
    
    /* Output */
    int16_t sample_left;
    int16_t sample_right;
    
    /* Timing */
    uint32_t total_cycles;
    
    /* Debug */
    bool debug_enabled;
    bool debug_trace_enabled;
    uint32_t debug_capture_samples;
    uint32_t debug_max_samples;
} NESApu;

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
void nes_audio_reset(void* apu_ptr);

/**
 * @brief Read from audio register
 * @param ctx NES context
 * @param addr Register address ($4000-$4017)
 * @return Register value
 */
uint8_t nes_audio_read(NESContext* ctx, uint16_t addr);

/**
 * @brief Write to audio register
 * @param ctx NES context
 * @param addr Register address ($4000-$4017)
 * @param value Value to write
 */
void nes_audio_write(NESContext* ctx, uint16_t addr, uint8_t value);

/**
 * @brief Step audio subsystem
 * @param ctx NES context
 * @param cycles Number of cycles to step
 */
void nes_audio_step(NESContext* ctx, uint32_t cycles);

/**
 * @brief Advance frame-sequencer timing from DIV transitions
 * @param apu_ptr Audio state
 * @param old_div Divider value before the CPU step
 * @param new_div Divider value after the CPU step
 */
void nes_audio_div_tick(void* apu_ptr, uint16_t old_div, uint16_t new_div);

/**
 * @brief Handle a write to DIV, including any APU clock edge it causes
 * @param apu_ptr Audio state
 * @param old_div Divider value before it was reset to 0
 */
void nes_audio_div_reset(void* apu_ptr, uint16_t old_div);

/**
 * @brief Get current sample for left/right channels
 * @param apu_ptr Audio state
 * @param left Pointer to store left sample
 * @param right Pointer to store right sample
 */
void nes_audio_get_samples(void* apu_ptr, int16_t* left, int16_t* right);

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

/**
 * @brief Handle frame IRQ from APU
 * @param apu_ptr Audio state
 */
void nes_apu_frame_irq(void* apu_ptr);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
