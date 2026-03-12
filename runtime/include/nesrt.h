/**
 * @file nesrt.h
 * @brief NES Runtime Library
 *
 * This runtime library provides the execution environment for recompiled
 * NES games. It implements memory access, CPU context, and hardware
 * emulation needed by the generated C code.
 */

#ifndef NESRT_H
#define NESRT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
/* ============================================================================
 * Configuration
 * ========================================================================== */

/**
 * @brief NES model selection
 */
typedef enum {
    NES_MODEL_NES,   /**< Original NES (NTSC) */
    NES_MODEL_FAMICOM,   /**< Famicom */
    NES_MODEL_DENDY,   /**< Dendy (PAL-like) */
} NESModel;

/**
 * @brief Runtime configuration
 */
typedef struct {
    NESModel model;
    bool enable_bootrom;
    bool enable_audio;
    bool enable_serial;
    uint32_t speed_percent; /**< 100 = normal, 200 = 2x, etc */
} NESConfig;

/* ============================================================================
 * Debugging
 * ========================================================================== */

extern bool nesrt_trace_enabled;
extern uint64_t nesrt_instruction_count;
extern uint64_t nesrt_instruction_limit;


/* ============================================================================
 * CPU Context
 * ========================================================================== */

/**
 * @brief Forward declaration
 */
typedef struct NESContext NESContext;

/**
 * @brief Platform callbacks for I/O and rendering
 */
typedef struct {
    void (*on_vblank)(NESContext* ctx, const uint8_t* framebuffer);
    void (*on_audio_sample)(NESContext* ctx, int16_t left, int16_t right);
    uint8_t (*get_joypad)(NESContext* ctx);
    void (*on_serial_byte)(NESContext* ctx, uint8_t byte);

    /* Save Data / External RAM */
    bool (*load_battery_ram)(NESContext* ctx, const char* rom_name, void* data, size_t size);
    bool (*save_battery_ram)(NESContext* ctx, const char* rom_name, const void* data, size_t size);
} NESPlatformCallbacks;

/**
 * @brief CPU register and state context
 *
 * This structure is passed to all recompiled functions and contains
 * the current state of the emulated CPU.
 */
typedef struct NESContext {
    /* 8-bit registers */
    union {
        struct { uint8_t f, a; };  /**< AF register pair (little-endian) */
        uint16_t af;
    };
    union {
        struct { uint8_t c, b; };  /**< BC register pair */
        uint16_t bc;
    };
    union {
        struct { uint8_t e, d; };  /**< DE register pair */
        uint16_t de;
    };
    union {
        struct { uint8_t l, h; };  /**< HL register pair */
        uint16_t hl;
    };

    /* Stack pointer and program counter */
    uint16_t sp;
    uint16_t pc;

    /* 6502 Index Registers */
    uint8_t x;      /**< X index register */
    uint8_t y;      /**< Y index register */

    /* Flag bits (unpacked for performance) */
    uint8_t f_z;  /**< Zero flag */
    uint8_t f_n;  /**< Negative flag (bit 7) */
    uint8_t f_v;  /**< Overflow flag (bit 6) - 6502 specific */
    uint8_t f_b;  /**< Break flag (bit 4) - for status register push */
    uint8_t f_d;  /**< Decimal mode flag (bit 3) */
    uint8_t f_i;  /**< Interrupt disable flag (bit 2) */
    uint8_t f_h;  /**< Half-carry flag (for GameBoy compatibility) */
    uint8_t f_c;  /**< Carry flag (bit 0) */

    /* Interrupt state */
    uint8_t ime;          /**< Interrupt Master Enable */
    uint8_t ime_pending;  /**< IME will be enabled after next instruction */
    uint8_t halted;       /**< CPU is halted */
    uint8_t stopped;      /**< CPU is stopped */
    uint8_t halt_bug;     /**< HALT bug: next instruction byte read twice */

    /* OAM DMA state */
    struct {
        uint8_t active;         /**< DMA is in progress */
        uint8_t source_high;    /**< Source address >> 8 */
        uint8_t progress;       /**< Bytes copied (0-159) */
        uint16_t cycles_remaining; /**< Cycles until DMA completes */
    } dma;

    /* Current bank numbers */
    uint16_t rom_bank;    /**< Current ROM bank (0x4000-0x7FFF) - 9 bits for MBC5 */
    uint8_t ram_bank;     /**< Current RAM bank */
    uint8_t wram_bank;    /**< Current WRAM bank (CGB only) */
    uint8_t vram_bank;    /**< Current VRAM bank (CGB only) */

    /* MBC state */
    uint8_t mbc_type;
    uint8_t ram_enabled;
    uint8_t mbc_mode;       /**< Banking mode for MBC1 (0=ROM, 1=RAM/Advanced) */
    uint8_t rom_bank_upper; /**< MBC1: Upper 2 bits of ROM bank / RAM bank selector */
    uint8_t rtc_mode;       /**< 0=RAM, 1=RTC registers (for 0xA000-0xBFFF) */
    uint8_t rtc_reg;        /**< Selected RTC register (0x08-0x0C) */

    /* Timing */
    uint32_t cycles;      /**< Cycles executed */
    uint32_t frame_cycles;/**< Cycles this frame */
    uint32_t last_sync_cycles; /**< Last cycles count synchronized with hardware */
    uint8_t  frame_done;  /**< Frame is finished and rendered */

    /* Timer internal state */
    uint16_t div_counter;   /**< Internal 16-bit divider counter */

    /* Memory pointers */
    uint8_t* rom;         /**< ROM data */
    size_t rom_size;
    uint8_t* eram;        /**< External RAM */
    size_t eram_size;
    uint8_t* wram;        /**< Work RAM */
    uint8_t* vram;        /**< Video RAM */
    uint8_t* oam;         /**< Object Attribute Memory */
    uint8_t* hram;        /**< High RAM (0xFF80-0xFFFE) */
    uint8_t* io;          /**< I/O registers (0xFF00-0xFF7F) */

    /* RTC state (MBC3) */
    struct {
        uint8_t s, m, h, dl, dh;        /**< Seconds, Minutes, Hours, Days Low, Days High */
        uint8_t latched_s, latched_m, latched_h, latched_dl, latched_dh;
        uint8_t latch_state;            /**< 0=Normal, 1=Latch prepared (wrote 0) */
        uint64_t last_time;             /**< Last time update (in cycles) */
        bool active;                    /**< RTC oscillator active (DH bit 6) */
    } rtc;

    /* Hardware components (opaque pointers) */
    void* ppu;            /**< Pixel Processing Unit */
    void* apu;            /**< Audio Processing Unit */
    void* timer;          /**< Timer unit */
    void* serial;         /**< Serial port */
    void* joypad;         /**< Joypad input */
    uint8_t last_joypad;  /**< Last joypad state for interrupt generation */

    /* Platform interface */
    void* platform;       /**< Platform-specific data */
    NESPlatformCallbacks callbacks; /**< Platform callbacks */

    /* Trace context */
    void* trace_file;     /**< FILE* for trace output */
    bool trace_entries_enabled;
} NESContext;

/* ============================================================================
 * Context Management
 * ========================================================================== */

/**
 * @brief Create a new NES context
 * @param config Configuration settings
 * @return New context or NULL on failure
 */
NESContext* nes_context_create(const NESConfig* config);

/**
 * @brief Destroy a NES context
 * @param ctx Context to destroy
 */
void nes_context_destroy(NESContext* ctx);

/**
 * @brief Reset the CPU state
 * @param ctx Context to reset
 * @param skip_bootrom If true, initialize to post-bootrom state
 */
void nes_context_reset(NESContext* ctx, bool skip_bootrom);

/**
 * @brief Load a ROM into the context
 * @param ctx Target context
 * @param data ROM data
 * @param size ROM size in bytes
 * @return true on success
 */
bool nes_context_load_rom(NESContext* ctx, const uint8_t* data, size_t size);

/**
 * @brief Save battery-backed RAM to persistent storage
 * @param ctx Target context
 * @return true on success
 */
bool nes_context_save_ram(NESContext* ctx);

/* ============================================================================
 * Memory Access
 * ========================================================================== */

/**
 * @brief Read a byte from memory
 * @param ctx CPU context
 * @param addr 16-bit address
 * @return Byte at address
 */
uint8_t nes_read8(NESContext* ctx, uint16_t addr);

/**
 * @brief Write a byte to memory
 * @param ctx CPU context
 * @param addr 16-bit address
 * @param value Byte to write
 */
void nes_write8(NESContext* ctx, uint16_t addr, uint8_t value);

/**
 * @brief Read a 16-bit word from memory (little-endian)
 * @param ctx CPU context
 * @param addr 16-bit address
 * @return Word at address
 */
uint16_t nes_read16(NESContext* ctx, uint16_t addr);

/**
 * @brief Write a 16-bit word to memory (little-endian)
 * @param ctx CPU context
 * @param addr 16-bit address
 * @param value Word to write
 */
void nes_write16(NESContext* ctx, uint16_t addr, uint16_t value);

/* ============================================================================
 * Stack Operations
 * ========================================================================== */

/**
 * @brief Push a 16-bit value onto the stack
 */
void nes_push16(NESContext* ctx, uint16_t value);

/**
 * @brief Pop a 16-bit value from the stack
 */
uint16_t nes_pop16(NESContext* ctx);

/* ============================================================================
 * ALU Operations (with flag updates)
 * ========================================================================== */

void nes_add8(NESContext* ctx, uint8_t value);
void nes_adc8(NESContext* ctx, uint8_t value);
void nes_sub8(NESContext* ctx, uint8_t value);
void nes_sbc8(NESContext* ctx, uint8_t value);
void nes_and8(NESContext* ctx, uint8_t value);
void nes_or8(NESContext* ctx, uint8_t value);
void nes_xor8(NESContext* ctx, uint8_t value);
void nes_cp8(NESContext* ctx, uint8_t value);
uint8_t nes_inc8(NESContext* ctx, uint8_t value);
uint8_t nes_dec8(NESContext* ctx, uint8_t value);

void nes_add16(NESContext* ctx, uint16_t value);
void nes_add_sp(NESContext* ctx, int8_t offset);
void nes_ld_hl_sp_n(NESContext* ctx, int8_t offset);

/* ============================================================================
 * 6502-specific ALU Operations
 * ========================================================================== */

/**
 * @brief 6502 ADC - Add with Carry (sets N, V, Z, C flags)
 * @param ctx CPU context
 * @param value Value to add
 */
void nes6502_adc(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 SBC - Subtract with Carry (sets N, V, Z, C flags)
 * @param ctx CPU context
 * @param value Value to subtract
 */
void nes6502_sbc(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 BIT - Bit Test (sets N, V, Z flags based on memory)
 * @param ctx CPU context
 * @param value Value to test
 */
void nes6502_bit(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 CMP - Compare A with value (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Value to compare
 */
void nes6502_cmp(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 CPX - Compare X with value (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Value to compare
 */
void nes6502_cpx(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 CPY - Compare Y with value (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Value to compare
 */
void nes6502_cpy(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 AND - Logical AND (sets N, Z flags)
 * @param ctx CPU context
 * @param value Value to AND
 */
void nes6502_and(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 ORA - Logical OR (sets N, Z flags)
 * @param ctx CPU context
 * @param value Value to OR
 */
void nes6502_ora(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 EOR - Logical XOR (sets N, Z flags)
 * @param ctx CPU context
 * @param value Value to XOR
 */
void nes6502_eor(NESContext* ctx, uint8_t value);

/**
 * @brief 6502 ASL - Arithmetic Shift Left (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Pointer to value to shift (in-place)
 */
void nes6502_asl(NESContext* ctx, uint8_t* value);

/**
 * @brief 6502 LSR - Logical Shift Right (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Pointer to value to shift (in-place)
 */
void nes6502_lsr(NESContext* ctx, uint8_t* value);

/**
 * @brief 6502 ROL - Rotate Left (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Pointer to value to rotate (in-place)
 */
void nes6502_rol(NESContext* ctx, uint8_t* value);

/**
 * @brief 6502 ROR - Rotate Right (sets N, Z, C flags)
 * @param ctx CPU context
 * @param value Pointer to value to rotate (in-place)
 */
void nes6502_ror(NESContext* ctx, uint8_t* value);

/**
 * @brief Set N and Z flags based on value
 * @param ctx CPU context
 * @param value Value to test
 */
static inline void nes6502_set_nz(NESContext* ctx, uint8_t value) {
    ctx->f_n = (value & 0x80) != 0;
    ctx->f_z = (value == 0);
}

/**
 * @brief Set status register from packed value (for PLP/RTI)
 * @param ctx CPU context
 * @param sr Packed status register value
 */
void nes6502_set_sr(NESContext* ctx, uint8_t sr);

/**
 * @brief Get packed status register (for PHP/RTI)
 * @param ctx CPU context
 * @return Packed status register value
 */
uint8_t nes6502_get_sr(NESContext* ctx);

/* ============================================================================
 * Rotate/Shift Operations
 * ========================================================================== */

uint8_t nes_rlc(NESContext* ctx, uint8_t value);
uint8_t nes_rrc(NESContext* ctx, uint8_t value);
uint8_t nes_rl(NESContext* ctx, uint8_t value);
uint8_t nes_rr(NESContext* ctx, uint8_t value);
uint8_t nes_sla(NESContext* ctx, uint8_t value);
uint8_t nes_sra(NESContext* ctx, uint8_t value);
uint8_t nes_srl(NESContext* ctx, uint8_t value);
uint8_t nes_swap(NESContext* ctx, uint8_t value);

void nes_rlca(NESContext* ctx);
void nes_rrca(NESContext* ctx);
void nes_rla(NESContext* ctx);
void nes_rra(NESContext* ctx);

/* ============================================================================
 * Bit Operations
 * ========================================================================== */

void nes_bit(NESContext* ctx, uint8_t bit, uint8_t value);

/* ============================================================================
 * Misc Operations
 * ========================================================================== */

void nes_daa(NESContext* ctx);

/* ============================================================================
 * Control Flow
 * ========================================================================== */

/**
 * @brief Call a function at the given address
 */
void nes_call(NESContext* ctx, uint16_t addr);

/**
 * @brief Return from a function
 */
void nes_ret(NESContext* ctx);

/**
 * @brief RST vector call
 */
void nes_rst(NESContext* ctx, uint8_t vector);

/**
 * @brief Jump to address in HL (JP HL)
 */
void nesrt_jump_hl(NESContext* ctx);

/**
 * @brief Dispatch to recompiled function at address
 */
void nes_dispatch(NESContext* ctx, uint16_t addr);

/**
 * @brief Dispatch a CALL to unanalyzed code (pushes return address first)
 */
void nes_dispatch_call(NESContext* ctx, uint16_t addr);

/**
 * @brief Fallback interpreter for uncompiled code
 */
void nes_interpret(NESContext* ctx, uint16_t addr);

/* ============================================================================
 * CPU State
 * ========================================================================== */

/**
 * @brief Halt the CPU until interrupt
 */
void nes_halt(NESContext* ctx);

/**
 * @brief Stop the CPU (and LCD)
 */
void nes_stop(NESContext* ctx);

/* ============================================================================
 * Flag Helpers
 * ========================================================================== */

/**
 * @brief Pack individual flags into F register
 */
static inline void nes_pack_flags(NESContext* ctx) {
    ctx->f = (ctx->f_z ? 0x80 : 0) |
             (ctx->f_n ? 0x40 : 0) |
             (ctx->f_h ? 0x20 : 0) |
             (ctx->f_c ? 0x10 : 0);
}



/**
 * @brief Unpack F register into individual flags
 */
static inline void nes_unpack_flags(NESContext* ctx) {
    ctx->f_z = (ctx->f & 0x80) != 0;
    ctx->f_n = (ctx->f & 0x40) != 0;
    ctx->f_h = (ctx->f & 0x20) != 0;
    ctx->f_c = (ctx->f & 0x10) != 0;
}

/* ============================================================================
 * Timing
 * ========================================================================== */

/**
 * @brief Add cycles to the timing counters
 */
void nes_add_cycles(NESContext* ctx, uint32_t cycles);

/**
 * @brief Check if a frame worth of cycles has elapsed
 */
bool nes_frame_complete(NESContext* ctx);

/**
 * @brief Get the current framebuffer
 * @param ctx CPU context
 * @return Pointer to 160x144 ARGB8888 framebuffer, or NULL if not ready
 */
const uint32_t* nes_get_framebuffer(NESContext* ctx);

/**
 * @brief Reset the frame ready flag for the next frame
 * @param ctx CPU context
 */
void nes_reset_frame(NESContext* ctx);

/**
 * @brief Process hardware for the given number of cycles
 */
void nes_tick(NESContext* ctx, uint32_t cycles);

/* ============================================================================
 * Platform Interface
 * ========================================================================== */

/* Moved NESPlatformCallbacks definition to top to resolve circular dependency */

/**
 * @brief Set platform callbacks
 */
void nes_set_platform_callbacks(NESContext* ctx, const NESPlatformCallbacks* callbacks);

/* ============================================================================
 * Execution
 * ========================================================================== */

/**
 * @brief Run one frame of emulation
 * @return Number of cycles executed
 */
uint32_t nes_run_frame(NESContext* ctx);

/**
 * @brief Run a single step (one instruction or until interrupt)
 * @return Number of cycles executed
 */
uint32_t nes_step(NESContext* ctx);

/**
 * @brief Helper to invoke audio callback
 */
void nes_audio_callback(NESContext* ctx, int16_t left, int16_t right);

/**
 * @brief Set input automation script (format: "frame:buttons:duration,...")
 */
void nes_platform_set_input_script(const char* script);

/**
 * @brief Set frames to dump screenshots (format: "frame1,frame2,...")
 */
void nes_platform_set_dump_frames(const char* frames);

/**
 * @brief Set filename prefix for screenshots
 */
void nes_platform_set_screenshot_prefix(const char* prefix);

/**
 * @brief Enable entry tracing to a file
 */
void nesrt_set_trace_file(const char* filename);

/**
 * @brief Log an entry point to the trace file
 */
void nesrt_log_trace(NESContext* ctx, uint16_t bank, uint16_t addr);

#ifdef __cplusplus
}
#endif

#endif /* NESRT_H */
