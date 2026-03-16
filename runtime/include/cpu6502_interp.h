/**
 * @file cpu6502_interp.h
 * @brief 6502 CPU interpreter API
 */

#ifndef CPU6502_INTERP_H
#define CPU6502_INTERP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize 6502 CPU interpreter
 */
void nes6502_init(void);

/**
 * @brief Reset 6502 CPU to reset vector
 * @param ctx NES context
 */
void nes6502_reset(void* ctx);

/**
 * @brief Execute one 6502 instruction
 * @param ctx NES context
 */
void nes6502_step(void* ctx);

/**
 * @brief Set program counter
 * @param pc New PC value
 */
void nes6502_set_pc(uint16_t pc);

/**
 * @brief Get program counter
 * @return Current PC value
 */
uint16_t nes6502_get_pc(void);

/**
 * @brief Get CPU registers for debugging
 * @param a Accumulator (output)
 * @param x X register (output)
 * @param y Y register (output)
 * @param sp Stack pointer (output)
 * @param flags Status flags (output)
 */
void nes6502_get_regs(uint8_t* a, uint8_t* x, uint8_t* y, uint8_t* sp, uint8_t* flags);

#ifdef __cplusplus
}
#endif

#endif /* CPU6502_INTERP_H */
