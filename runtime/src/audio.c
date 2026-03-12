/**
 * @file audio.c
 * @brief NES APU (Audio Processing Unit) implementation
 *
 * Implements the full NES APU with:
 * - 2 Pulse channels (square waves with duty cycle, envelope, sweep)
 * - 1 Triangle channel (linear counter, 16-step waveform)
 * - 1 Noise channel (LFSR-based, envelope)
 * - 1 DMC channel (delta modulation, stub)
 * - Frame counter (mode 0/1, ~240Hz)
 */

#include "audio.h"
#include "nesrt_debug.h"
#include "audio_stats.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Constants and Lookup Tables
 * ========================================================================== */

/* Length counter lookup table */
static const uint8_t g_length_counter_lookup[NES_APU_LENGTH_COUNTER_SIZE] = {
    /* HI/LO  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
    /* 0 */  10, 254,  20,   2,  40,   4,  80,   6, 160,   8,  60,  10,  14,  12,  26,  14,
    /* 1 */  12,  16,  24,  18,  48,  20,  96,  22, 192,  24,  72,  26,  16,  28,  32,  30
};

/* Duty cycle patterns (8 steps) */
static const uint8_t g_duty_patterns[4][NES_APU_DUTY_PATTERN_SIZE] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, /* 12.5% duty */
    {0, 1, 1, 0, 0, 0, 0, 0}, /* 25% duty */
    {0, 1, 1, 1, 1, 0, 0, 0}, /* 50% duty */
    {1, 0, 0, 1, 1, 1, 1, 1}  /* 25% duty (negated) */
};

/* Triangle waveform sequence (32 steps) */
static const uint8_t g_triangle_sequence[NES_APU_TRIANGLE_SEQUENCE_SIZE] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

/* Noise period lookup (NTSC) */
static const uint16_t g_noise_period_lookup_ntsc[NES_APU_NOISE_PERIOD_SIZE] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

/* DMC rate lookup (NTSC) */
static const uint16_t g_dmc_rate_ntsc[NES_APU_DMC_RATE_SIZE] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

/* Look-up tables for pulse and TND (triangle, noise, DMC) mixing */
static float g_pulse_lut[NES_APU_PULSE_LUT_SIZE];
static float g_tnd_lut[NES_APU_TND_LUT_SIZE];

/* Debug file for audio capture */
static FILE* g_debug_audio_file = NULL;
static FILE* g_debug_trace_file = NULL;
static uint32_t g_debug_max_samples = 0;

/* ============================================================================
 * Helper Functions - Internal
 * ========================================================================== */

/**
 * @brief Compute mixer look-up tables
 */
static void compute_mixer_lut(void) {
    /* Pulse channel LUT (0-15) */
    g_pulse_lut[0] = 0.0f;
    for (int i = 1; i < NES_APU_PULSE_LUT_SIZE; i++) {
        g_pulse_lut[i] = 95.52f / (8128.0f / (float)i + 100.0f);
    }
    
    /* TND LUT (0-202) */
    g_tnd_lut[0] = 0.0f;
    for (int i = 1; i < NES_APU_TND_LUT_SIZE; i++) {
        g_tnd_lut[i] = 163.67f / (24329.0f / (float)i + 100.0f);
    }
}

/**
 * @brief Initialize pulse channel
 */
static void init_pulse(NESApuPulse* pulse, uint8_t id) {
    memset(pulse, 0, sizeof(NESApuPulse));
    pulse->id = id;
    pulse->timer.limit = 7;
    pulse->timer.from = 0;
    pulse->timer.loop = 1;
    pulse->sweep.limit = 0;
    pulse->envelope.limit = 7;
    pulse->envelope.from = 0;
    pulse->envelope.loop = 1;
}

/**
 * @brief Initialize triangle channel
 */
static void init_triangle(NESApuTriangle* triangle) {
    memset(triangle, 0, sizeof(NESApuTriangle));
    triangle->timer.limit = 31;
    triangle->timer.from = 0;
    triangle->halt = 1;
}

/**
 * @brief Initialize noise channel
 */
static void init_noise(NESApuNoise* noise) {
    memset(noise, 0, sizeof(NESApuNoise));
    noise->timer.from = 0;
    noise->shift_register = 1;
    noise->envelope.limit = 7;
    noise->envelope.from = 0;
    noise->envelope.loop = 1;
}

/**
 * @brief Initialize DMC channel
 */
static void init_dmc(NESApuDmc* dmc) {
    memset(dmc, 0, sizeof(NESApuDmc));
    dmc->empty = 1;
    dmc->silence = 1;
}

/**
 * @brief Clock a divider (returns 1 on overflow)
 */
static uint8_t clock_divider(NESApuDivider* divider) {
    if (divider->counter > 0) {
        divider->counter--;
        return 0;
    }
    
    divider->counter = divider->period;
    divider->step++;
    if (divider->limit > 0 && divider->step > divider->limit) {
        divider->step = divider->from;
    }
    return 1;
}

/**
 * @brief Clock envelope divider (inverse - counts down)
 */
static uint8_t clock_envelope(NESApuDivider* divider) {
    if (divider->counter > 0) {
        divider->counter--;
        return 0;
    }
    
    divider->counter = divider->period;
    if (divider->limit > 0 && divider->step == 0 && divider->loop) {
        divider->step = divider->limit;
    } else if (divider->step > 0) {
        divider->step--;
    }
    return 1;
}

/**
 * @brief Clock pulse length and sweep
 */
static void clock_pulse_length_sweep(NESApuPulse* pulse) {
    /* Sweep reload */
    if (pulse->sweep_reload) {
        pulse->sweep_reload = 0;
        pulse->sweep.counter = 0;
    }
    
    /* Clock sweep */
    if (clock_divider(&pulse->sweep)) {
        if (pulse->sweep_enabled && pulse->sweep_shift > 0 && !pulse->sweep_mute) {
            /* Calculate swept period */
            uint16_t shift = pulse->timer.period >> pulse->sweep_shift;
            int16_t change = pulse->sweep_neg ? -shift - (pulse->id == 2 ? 0 : 1) : shift;
            int16_t new_period = pulse->timer.period + change;
            
            if (new_period < 8 || new_period > 0x7FF) {
                pulse->sweep_mute = 1;
            } else {
                pulse->timer.period = (uint16_t)new_period;
                pulse->sweep_mute = 0;
            }
        }
    }
    
    /* Clock length counter */
    if (pulse->length_counter > 0 && !pulse->envelope.loop) {
        pulse->length_counter--;
    }
}

/**
 * @brief Clock triangle channel
 */
static uint8_t clock_triangle(NESApuTriangle* triangle) {
    if (triangle->timer.counter > 0) {
        triangle->timer.counter--;
        return 0;
    }
    
    triangle->timer.counter = triangle->timer.period;
    
    /* Only advance sequencer if both counters are non-zero */
    if (triangle->length_counter > 0 && triangle->linear_counter > 0) {
        triangle->sequencer_step++;
        if (triangle->sequencer_step >= NES_APU_TRIANGLE_SEQUENCE_SIZE) {
            triangle->sequencer_step = 0;
        }
    }
    
    return 1;
}

/**
 * @brief Quarter frame processing (envelopes, linear counter)
 */
static void quarter_frame(NESApu* apu) {
    /* Clock envelopes */
    if (clock_envelope(&apu->pulse1.envelope)) {
        if (apu->pulse1.envelope_started) {
            if (apu->pulse1.envelope.step > 0) {
                apu->pulse1.envelope_volume--;
            } else if (apu->pulse1.envelope.loop) {
                apu->pulse1.envelope_volume = 15;
            }
        } else {
            apu->pulse1.envelope_volume = 15;
            apu->pulse1.envelope_started = 1;
        }
    }
    
    if (clock_envelope(&apu->pulse2.envelope)) {
        if (apu->pulse2.envelope_started) {
            if (apu->pulse2.envelope.step > 0) {
                apu->pulse2.envelope_volume--;
            } else if (apu->pulse2.envelope.loop) {
                apu->pulse2.envelope_volume = 15;
            }
        } else {
            apu->pulse2.envelope_volume = 15;
            apu->pulse2.envelope_started = 1;
        }
    }
    
    if (clock_envelope(&apu->noise.envelope)) {
        if (apu->noise.envelope_started) {
            if (apu->noise.envelope.step > 0) {
                apu->noise.envelope_volume--;
            } else if (apu->noise.envelope.loop) {
                apu->noise.envelope_volume = 15;
            }
        } else {
            apu->noise.envelope_volume = 15;
            apu->noise.envelope_started = 1;
        }
    }
    
    /* Clock triangle linear counter */
    NESApuTriangle* tri = &apu->triangle;
    if (tri->linear_reload_flag) {
        tri->linear_counter = tri->linear_reload;
    } else if (tri->linear_counter > 0) {
        tri->linear_counter--;
    }
    
    /* Clear reload flag if halt is clear */
    if (!tri->halt) {
        tri->linear_reload_flag = 0;
    }
}

/**
 * @brief Half frame processing (length counters, sweep)
 */
static void half_frame(NESApu* apu) {
    clock_pulse_length_sweep(&apu->pulse1);
    clock_pulse_length_sweep(&apu->pulse2);
    
    /* Triangle length counter */
    if (!apu->triangle.halt && apu->triangle.length_counter > 0) {
        apu->triangle.length_counter--;
    }
    
    /* Noise length counter */
    if (apu->noise.length_counter > 0 && !apu->noise.envelope.loop) {
        apu->noise.length_counter--;
    }
}

/**
 * @brief Clock DMC channel
 */
static void clock_dmc(NESApu* apu) {
    NESApuDmc* dmc = &apu->dmc;
    
    if (!dmc->enabled) return;
    
    /* Sample buffer refill */
    if (dmc->enabled && dmc->empty) {
        if (dmc->bytes_remaining > 0) {
            /* Read sample from memory */
            dmc->sample = nes_read8((NESContext*)NULL, dmc->current_addr);
            dmc->empty = 0;
            dmc->bytes_remaining--;
            
            if (dmc->current_addr >= 0xFFFF) {
                dmc->current_addr = 0x8000;
            } else {
                dmc->current_addr++;
            }
            
            dmc->irq_set = 0;
        } else if (dmc->bytes_remaining == 0) {
            if (dmc->loop) {
                dmc->current_addr = dmc->sample_addr;
                dmc->bytes_remaining = dmc->sample_length;
            } else if (dmc->irq_enable && !dmc->irq_set) {
                dmc->interrupt = 1;
                dmc->irq_set = 1;
                /* Note: IRQ would be triggered in emulator context */
            }
        }
    }
    
    /* Rate counter */
    if (dmc->rate_index > 0) {
        dmc->rate_index--;
        return;
    }
    dmc->rate_index = dmc->rate;
    
    /* Output unit */
    if (dmc->bits_remaining > 0) {
        if (!dmc->silence) {
            if (dmc->bits & 1) {
                if (dmc->counter < 127) {
                    dmc->counter += 2;
                }
            } else if (dmc->counter > 1) {
                dmc->counter -= 2;
            }
            dmc->bits >>= 1;
        }
        dmc->bits_remaining--;
    }
    
    if (dmc->bits_remaining == 0) {
        if (dmc->empty) {
            dmc->silence = 1;
        } else {
            dmc->bits = dmc->sample;
            dmc->empty = 1;
            dmc->silence = 0;
        }
        dmc->bits_remaining = 8;
    }
}

/**
 * @brief Generate audio sample from all channels
 */
static float generate_sample(NESApu* apu) {
    /* Pulse 1 output */
    uint8_t pulse1_output = 0;
    if (apu->pulse1.enabled && apu->pulse1.length_counter > 0 && 
        !apu->pulse1.sweep_mute && apu->pulse1.timer.period > 0) {
        uint8_t duty_index = apu->pulse1.timer.step & 0x07;
        uint8_t duty_value = g_duty_patterns[apu->pulse1.duty][duty_index];
        if (duty_value) {
            pulse1_output = (apu->pulse1.ctrl & 0x0F) != 0 ? 
                apu->pulse1.envelope_volume : 
                (apu->pulse1.ctrl >> 1) & 0x0F;
        }
    }
    
    /* Pulse 2 output */
    uint8_t pulse2_output = 0;
    if (apu->pulse2.enabled && apu->pulse2.length_counter > 0 && 
        !apu->pulse2.sweep_mute && apu->pulse2.timer.period > 0) {
        uint8_t duty_index = apu->pulse2.timer.step & 0x07;
        uint8_t duty_value = g_duty_patterns[apu->pulse2.duty][duty_index];
        if (duty_value) {
            pulse2_output = (apu->pulse2.ctrl & 0x0F) != 0 ? 
                apu->pulse2.envelope_volume : 
                (apu->pulse2.ctrl >> 1) & 0x0F;
        }
    }
    
    /* Triangle output */
    uint8_t triangle_output = 0;
    if (apu->triangle.enabled && apu->triangle.length_counter > 0 && 
        apu->triangle.linear_counter > 0) {
        triangle_output = g_triangle_sequence[apu->triangle.sequencer_step];
    }
    
    /* Noise output */
    uint8_t noise_output = 0;
    if (apu->noise.enabled && apu->noise.length_counter > 0) {
        uint8_t noise_bit = apu->noise.shift_register & 1;
        if (!noise_bit) {
            noise_output = (apu->noise.ctrl & 0x0F) != 0 ? 
                apu->noise.envelope_volume : 
                (apu->noise.ctrl >> 1) & 0x0F;
        }
    }
    
    /* DMC output */
    uint8_t dmc_output = apu->dmc.counter;
    
    /* Mix using look-up tables */
    /* Pulse channels are mixed together */
    int pulse_total = pulse1_output + pulse2_output;
    if (pulse_total > NES_APU_PULSE_LUT_SIZE - 1) {
        pulse_total = NES_APU_PULSE_LUT_SIZE - 1;
    }
    float pulse_sample = g_pulse_lut[pulse_total];
    
    /* TND (Triangle, Noise, DMC) are mixed together */
    int tnd_total = triangle_output * 3 + noise_output * 2 + dmc_output;
    if (tnd_total > NES_APU_TND_LUT_SIZE - 1) {
        tnd_total = NES_APU_TND_LUT_SIZE - 1;
    }
    float tnd_sample = g_tnd_lut[tnd_total];
    
    /* Combine and scale to 16-bit range */
    float combined = (pulse_sample + tnd_sample) * 32000.0f / 100.0f;
    
    return combined;
}

/**
 * @brief Clock noise LFSR
 */
static void clock_noise_lfsr(NESApuNoise* noise) {
    uint8_t feedback = (noise->shift_register & 1) ^ 
                       ((noise->shift_register >> (noise->mode ? 6 : 1)) & 1);
    noise->shift_register >>= 1;
    noise->shift_register |= feedback ? 0x4000 : 0;
}

/* ============================================================================
 * Public Interface Implementation
 * ========================================================================== */

void* nes_audio_create(void) {
    NESApu* apu = (NESApu*)calloc(1, sizeof(NESApu));
    if (!apu) return NULL;
    
    compute_mixer_lut();
    
    init_pulse(&apu->pulse1, 1);
    init_pulse(&apu->pulse2, 2);
    init_triangle(&apu->triangle);
    init_noise(&apu->noise);
    init_dmc(&apu->dmc);
    
    apu->frame.mode = 0;
    apu->frame.step = 0;
    apu->frame.cycles = 0;
    apu->frame.irq_pending = 0;
    apu->frame.irq_inhibit = 0;
    
    apu->status = 0;
    apu->sample_left = 0;
    apu->sample_right = 0;
    apu->total_cycles = 0;
    apu->debug_enabled = false;
    apu->debug_trace_enabled = false;
    
    DBG_AUDIO("NES APU created");
    return apu;
}

void nes_audio_destroy(void* apu_ptr) {
    if (apu_ptr) {
        free(apu_ptr);
    }
    if (g_debug_audio_file) {
        fclose(g_debug_audio_file);
        g_debug_audio_file = NULL;
    }
    if (g_debug_trace_file) {
        fclose(g_debug_trace_file);
        g_debug_trace_file = NULL;
    }
    DBG_AUDIO("NES APU destroyed");
}

void nes_audio_reset(void* apu_ptr) {
    NESApu* apu = (NESApu*)apu_ptr;
    if (!apu) return;
    
    /* Reset all channels */
    init_pulse(&apu->pulse1, 1);
    init_pulse(&apu->pulse2, 2);
    init_triangle(&apu->triangle);
    init_noise(&apu->noise);
    init_dmc(&apu->dmc);
    
    /* Reset frame sequencer */
    apu->frame.mode = 0;
    apu->frame.step = 0;
    apu->frame.cycles = 0;
    apu->frame.irq_pending = 0;
    apu->frame.irq_inhibit = 0;
    
    /* Reset status */
    apu->status = 0;
    apu->sample_left = 0;
    apu->sample_right = 0;
    apu->total_cycles = 0;
    
    DBG_AUDIO("NES APU reset");
}

uint8_t nes_audio_read(NESContext* ctx, uint16_t addr) {
    NESApu* apu = (NESApu*)ctx->apu;
    (void)ctx;
    
    switch (addr) {
        case 0x4015: /* APU Status/Control */
        {
            uint8_t status = 0;
            
            /* Channel enabled flags */
            status |= (apu->pulse1.length_counter > 0) ? 0x01 : 0x00;
            status |= (apu->pulse2.length_counter > 0) ? 0x02 : 0x00;
            status |= (apu->triangle.length_counter > 0) ? 0x04 : 0x00;
            status |= (apu->noise.length_counter > 0) ? 0x08 : 0x00;
            status |= (apu->dmc.bytes_remaining > 0) ? 0x10 : 0x00;
            
            /* Interrupt flags */
            status |= apu->dmc.interrupt ? 0x80 : 0x00;
            status |= apu->frame.irq_pending ? 0x40 : 0x00;
            
            /* Clear frame IRQ flag on read */
            apu->frame.irq_pending = 0;
            
            return status;
        }
        
        case 0x4016: /* Controller 1 - handled by joypad */
        case 0x4017: /* Controller 2 - handled by joypad */
            return 0x00;
            
        default:
            return 0x00;
    }
}

void nes_audio_write(NESContext* ctx, uint16_t addr, uint8_t value) {
    NESApu* apu = (NESApu*)ctx->apu;
    
    if (g_debug_trace_file) {
        fprintf(g_debug_trace_file, "APU WRITE: $%04X = $%02X\n", addr, value);
    }
    
    switch (addr) {
        /* Pulse 1 registers */
        case 0x4000: /* Pulse 1 control */
            apu->pulse1.ctrl = value;
            apu->pulse1.duty = (value >> 6) & 0x03;
            apu->pulse1.envelope.loop = (value & 0x20) == 0;
            apu->pulse1.envelope.period = value & 0x0F;
            apu->pulse1.envelope.step = apu->pulse1.envelope.period;
            break;
            
        case 0x4001: /* Pulse 1 sweep */
            apu->pulse1.ramp = value;
            apu->pulse1.sweep.period = ((value >> 4) & 0x07);
            apu->pulse1.sweep_enabled = (value & 0x80) != 0;
            apu->pulse1.sweep_neg = (value & 0x08) != 0;
            apu->pulse1.sweep_shift = value & 0x07;
            apu->pulse1.sweep_reload = 1;
            break;
            
        case 0x4002: /* Pulse 1 frequency low */
            apu->pulse1.freq = (apu->pulse1.freq & 0x0700) | value;
            apu->pulse1.timer.period = apu->pulse1.freq;
            break;
            
        case 0x4003: /* Pulse 1 frequency high / length */
            apu->pulse1.freq = (apu->pulse1.freq & 0x00FF) | ((value & 0x07) << 8);
            apu->pulse1.timer.period = apu->pulse1.freq;
            
            if (apu->status & 0x01) {
                apu->pulse1.length_counter = g_length_counter_lookup[value >> 3];
            }
            apu->pulse1.envelope.step = apu->pulse1.envelope.period;
            apu->pulse1.envelope_started = 0;
            break;
            
        /* Pulse 2 registers */
        case 0x4004: /* Pulse 2 control */
            apu->pulse2.ctrl = value;
            apu->pulse2.duty = (value >> 6) & 0x03;
            apu->pulse2.envelope.loop = (value & 0x20) == 0;
            apu->pulse2.envelope.period = value & 0x0F;
            apu->pulse2.envelope.step = apu->pulse2.envelope.period;
            break;
            
        case 0x4005: /* Pulse 2 sweep */
            apu->pulse2.ramp = value;
            apu->pulse2.sweep.period = ((value >> 4) & 0x07);
            apu->pulse2.sweep_enabled = (value & 0x80) != 0;
            apu->pulse2.sweep_neg = (value & 0x08) != 0;
            apu->pulse2.sweep_shift = value & 0x07;
            apu->pulse2.sweep_reload = 1;
            break;
            
        case 0x4006: /* Pulse 2 frequency low */
            apu->pulse2.freq = (apu->pulse2.freq & 0x0700) | value;
            apu->pulse2.timer.period = apu->pulse2.freq;
            break;
            
        case 0x4007: /* Pulse 2 frequency high / length */
            apu->pulse2.freq = (apu->pulse2.freq & 0x00FF) | ((value & 0x07) << 8);
            apu->pulse2.timer.period = apu->pulse2.freq;
            
            if (apu->status & 0x02) {
                apu->pulse2.length_counter = g_length_counter_lookup[value >> 3];
            }
            apu->pulse2.envelope.step = apu->pulse2.envelope.period;
            apu->pulse2.envelope_started = 0;
            break;
            
        /* Triangle registers */
        case 0x4008: /* Triangle control */
            apu->triangle.ctrl = value;
            apu->triangle.halt = (value & 0x80) != 0;
            apu->triangle.linear_reload = value & 0x7F;
            break;
            
        case 0x400A: /* Triangle frequency low */
            apu->triangle.freq = (apu->triangle.freq & 0x0700) | value;
            apu->triangle.timer.period = apu->triangle.freq;
            break;
            
        case 0x400B: /* Triangle frequency high / length */
            apu->triangle.freq = (apu->triangle.freq & 0x00FF) | ((value & 0x07) << 8);
            apu->triangle.timer.period = apu->triangle.freq;
            
            if (apu->status & 0x04) {
                apu->triangle.length_counter = g_length_counter_lookup[value >> 3];
            }
            apu->triangle.linear_reload_flag = 1;
            break;
            
        /* Noise registers */
        case 0x400C: /* Noise control */
            apu->noise.ctrl = value;
            apu->noise.envelope.loop = (value & 0x20) == 0;
            apu->noise.envelope.period = value & 0x0F;
            apu->noise.envelope.step = apu->noise.envelope.period;
            break;
            
        case 0x400E: /* Noise frequency */
            apu->noise.freq = value & 0x0F;
            apu->noise.mode = (value & 0x80) != 0;
            /* Period will be set in nes_audio_step based on NTSC/PAL */
            break;
            
        case 0x400F: /* Noise length */
            if (apu->status & 0x08) {
                apu->noise.length_counter = g_length_counter_lookup[value >> 3];
            }
            apu->noise.envelope.step = apu->noise.envelope.period;
            apu->noise.envelope_started = 0;
            break;
            
        /* DMC registers */
        case 0x4010: /* DMC control */
            apu->dmc.ctrl = value;
            apu->dmc.irq_enable = (value & 0x80) != 0;
            apu->dmc.loop = (value & 0x40) != 0;
            
            /* Set rate based on NTSC/PAL (default to NTSC) */
            apu->dmc.rate = g_dmc_rate_ntsc[value & 0x0F] - 1;
            break;
            
        case 0x4011: /* DMC direct load / counter */
            apu->dmc.da = value & 0x7F;
            apu->dmc.counter = apu->dmc.da;
            break;
            
        case 0x4012: /* DMC sample address */
            apu->dmc.addr = value;
            apu->dmc.sample_addr = 0xC000 + (uint16_t)value * 64;
            break;
            
        case 0x4013: /* DMC sample length */
            apu->dmc.len = value;
            apu->dmc.sample_length = (uint16_t)value * 16 + 1;
            break;
            
        /* APU status/control */
        case 0x4015: /* APU status */
            apu->status = value;
            
            /* Channel enable/disable */
            apu->pulse1.enabled = (value & 0x01) != 0;
            apu->pulse2.enabled = (value & 0x02) != 0;
            apu->triangle.enabled = (value & 0x04) != 0;
            apu->noise.enabled = (value & 0x08) != 0;
            apu->dmc.enabled = (value & 0x10) != 0;
            
            /* Reset length counters when enabling */
            if (!apu->pulse1.enabled) {
                apu->pulse1.length_counter = 0;
            }
            if (!apu->pulse2.enabled) {
                apu->pulse2.length_counter = 0;
            }
            if (!apu->triangle.enabled) {
                apu->triangle.length_counter = 0;
            }
            if (!apu->noise.enabled) {
                apu->noise.length_counter = 0;
            }
            if (!apu->dmc.enabled) {
                apu->dmc.interrupt = 0;
            }
            
            /* Clear frame IRQ */
            apu->frame.irq_pending = 0;
            break;
            
        case 0x4017: /* Frame counter control */
            apu->frame.mode = (value & 0x80) != 0 ? 1 : 0;
            apu->frame.irq_inhibit = (value & 0x40) != 0;
            
            /* Reset frame sequencer */
            apu->frame.cycles = 0;
            apu->frame.step = 0;
            
            /* In mode 1, clock immediately */
            if (apu->frame.mode) {
                quarter_frame(apu);
                half_frame(apu);
            }
            
            /* Clear frame IRQ */
            apu->frame.irq_pending = 0;
            break;
            
        default:
            break;
    }
}

void nes_audio_step(NESContext* ctx, uint32_t cycles) {
    NESApu* apu = (NESApu*)ctx->apu;
    if (!apu) return;
    
    apu->total_cycles += cycles;
    
    /* Update noise period based on frequency */
    apu->noise.timer.period = g_noise_period_lookup_ntsc[apu->noise.freq];
    
    /* Clock DMC */
    clock_dmc(apu);
    
    /* Clock noise LFSR on odd cycles */
    if (apu->total_cycles & 1) {
        if (clock_divider(&apu->noise.timer)) {
            clock_noise_lfsr(&apu->noise);
        }
    }
    
    /* Clock pulse timers */
    clock_divider(&apu->pulse1.timer);
    clock_divider(&apu->pulse2.timer);
    
    /* Clock triangle */
    clock_triangle(&apu->triangle);
    
    /* Frame sequencer (NTSC timing) */
    if (!apu->frame.irq_inhibit || apu->frame.mode == 0) {
        apu->frame.cycles += cycles;
        
        if (apu->frame.mode == 0) {
            /* 4-step sequence */
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_1) {
                quarter_frame(apu);
                apu->frame.cycles -= NES_APU_FRAME_STEP_1;
            }
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_2 - NES_APU_FRAME_STEP_1) {
                quarter_frame(apu);
                half_frame(apu);
                apu->frame.cycles -= (NES_APU_FRAME_STEP_2 - NES_APU_FRAME_STEP_1);
            }
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_3 - NES_APU_FRAME_STEP_2) {
                quarter_frame(apu);
                apu->frame.cycles -= (NES_APU_FRAME_STEP_3 - NES_APU_FRAME_STEP_2);
            }
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_4 - NES_APU_FRAME_STEP_3) {
                quarter_frame(apu);
                half_frame(apu);
                
                if (!apu->frame.irq_inhibit) {
                    apu->frame.irq_pending = 1;
                }
                apu->frame.cycles -= (NES_APU_FRAME_STEP_4 - NES_APU_FRAME_STEP_3);
            }
        } else {
            /* 5-step sequence (no IRQ) */
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_1) {
                quarter_frame(apu);
                apu->frame.cycles -= NES_APU_FRAME_STEP_1;
            }
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_2 - NES_APU_FRAME_STEP_1) {
                quarter_frame(apu);
                half_frame(apu);
                apu->frame.cycles -= (NES_APU_FRAME_STEP_2 - NES_APU_FRAME_STEP_1);
            }
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_3 - NES_APU_FRAME_STEP_2) {
                quarter_frame(apu);
                apu->frame.cycles -= (NES_APU_FRAME_STEP_3 - NES_APU_FRAME_STEP_2);
            }
            if (apu->frame.cycles >= NES_APU_FRAME_STEP_5 - NES_APU_FRAME_STEP_3) {
                quarter_frame(apu);
                half_frame(apu);
                apu->frame.cycles -= (NES_APU_FRAME_STEP_5 - NES_APU_FRAME_STEP_3);
            }
        }
    }
    
    /* Generate audio sample */
    float sample = generate_sample(apu);
    apu->sample_left = (int16_t)sample;
    apu->sample_right = (int16_t)sample;
    
    /* Debug audio capture */
    if (apu->debug_enabled && g_debug_audio_file) {
        int16_t sample_16 = (int16_t)(sample * 32767.0f);
        fwrite(&sample_16, sizeof(int16_t), 1, g_debug_audio_file);
        apu->debug_capture_samples++;
        
        if (apu->debug_capture_samples >= apu->debug_max_samples) {
            fclose(g_debug_audio_file);
            g_debug_audio_file = NULL;
            apu->debug_enabled = false;
            DBG_AUDIO("Debug audio capture complete (%u samples)", apu->debug_max_samples);
        }
    }
    
    /* Update audio stats */
    audio_stats_sample_generated();
}

void nes_audio_div_tick(void* apu_ptr, uint16_t old_div, uint16_t new_div) {
    /* NES APU doesn't use DIV like GB APU */
    (void)apu_ptr;
    (void)old_div;
    (void)new_div;
}

void nes_audio_div_reset(void* apu_ptr, uint16_t old_div) {
    /* NES APU doesn't use DIV like GB APU */
    (void)apu_ptr;
    (void)old_div;
}

void nes_audio_get_samples(void* apu_ptr, int16_t* left, int16_t* right) {
    NESApu* apu = (NESApu*)apu_ptr;
    
    if (apu) {
        *left = apu->sample_left;
        *right = apu->sample_right;
    } else {
        *left = 0;
        *right = 0;
    }
}

void nes_audio_set_debug(bool enabled) {
    if (enabled && !g_debug_audio_file) {
        g_debug_audio_file = fopen("debug_audio.raw", "wb");
        if (g_debug_audio_file) {
            DBG_AUDIO("Debug audio capture started: debug_audio.raw");
        }
    } else if (!enabled && g_debug_audio_file) {
        fclose(g_debug_audio_file);
        g_debug_audio_file = NULL;
        DBG_AUDIO("Debug audio capture stopped");
    }
}

void nes_audio_set_debug_capture_seconds(uint32_t seconds) {
    /* At 48kHz sample rate */
    g_debug_max_samples = seconds * NES_APU_SAMPLING_RATE;
    DBG_AUDIO("Debug audio capture length set to %u seconds (%u samples)", 
              seconds, g_debug_max_samples);
}

void nes_audio_set_debug_trace(bool enabled) {
    if (enabled && !g_debug_trace_file) {
        g_debug_trace_file = fopen("debug_audio_trace.log", "w");
        if (g_debug_trace_file) {
            DBG_AUDIO("APU trace logging started: debug_audio_trace.log");
        }
    } else if (!enabled && g_debug_trace_file) {
        fclose(g_debug_trace_file);
        g_debug_trace_file = NULL;
        DBG_AUDIO("APU trace logging stopped");
    }
}

void nes_apu_frame_irq(void* apu_ptr) {
    NESApu* apu = (NESApu*)apu_ptr;
    if (apu) {
        apu->frame.irq_pending = 1;
    }
}
