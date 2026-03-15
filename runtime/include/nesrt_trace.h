/**
 * @file nesrt_trace.h
 * @brief Call-stack tracing with tagging support for NES runtime
 *
 * This module provides call-stack tracking and event logging for debugging
 * recompiled NES games. It tracks JSR/RTS calls and logs hardware events
 * (PPU accesses, controller reads, bank switches) with full call context.
 *
 * @section usage Usage Example
 * @code
 * // Enable tracing and set output file
 * nesrt_trace_enable(1);
 * nesrt_trace_set_file("trace.log");
 *
 * // Set a tag for filtering
 * nesrt_set_tag("rendering");
 *
 * // In recompiled code - track JSR
 * void func_8000(NESContext* ctx) {
 *     nesrt_trace_push(ctx->pc);  // Push return address
 *     // ... function body ...
 *     nesrt_trace_pop();          // Pop on RTS
 * }
 *
 * // Log hardware events with call stack
 * nesrt_log_ppu_write(0x2000, value, nesrt_trace_depth());
 * @endcode
 *
 * @section output Output Format
 * Events are logged as:
 * @code
 * [TAGS] EVENT_TYPE: description (call stack)
 *
 * Example:
 * [rendering] PPU_WRITE: $2000 = $80 (depth=3: $8000 -> $8234 -> $8567)
 * @endcode
 */

#ifndef NESRT_TRACE_H
#define NESRT_TRACE_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration Constants
 * ========================================================================== */

/** Maximum call stack depth to track */
#define NESRT_MAX_STACK_DEPTH 32

/** Maximum number of active tags */
#define NESRT_MAX_TAGS 8

/** Maximum length of a single tag string */
#define NESRT_MAX_TAG_LEN 32

/** Maximum length of semicolon-separated tags string */
#define NESRT_MAX_TAGS_STR 256

/** Default max stack depth to log */
#define NESRT_TRACE_DEFAULT_MAX_DEPTH 5

/* ============================================================================
 * Data Structures
 * ========================================================================== */

/**
 * @brief Stack entry representing a call frame
 */
typedef struct {
    uint16_t pc;                    /**< Program counter at call time */
    const char* tag_str;            /**< Tags active at time of call */
} nesrt_stack_entry_t;

/**
 * @brief Global trace state
 *
 * This structure holds all tracing state including the call stack,
 * active tags, and configuration options.
 */
typedef struct {
    int enabled;                                    /**< Tracing enabled flag */
    int max_depth;                                  /**< Max stack depth to log */
    int stack_depth;                                /**< Current stack depth */
    nesrt_stack_entry_t stack[NESRT_MAX_STACK_DEPTH]; /**< Call stack */
    char tags[NESRT_MAX_TAGS][NESRT_MAX_TAG_LEN];   /**< Active tags */
    char tags_str[NESRT_MAX_TAGS_STR];              /**< Semicolon-separated tags */
    FILE* output_file;                              /**< Output file (NULL = stderr) */
    char tag_filter[NESRT_MAX_TAG_LEN];             /**< Tag filter (empty = all) */
} nesrt_trace_state_t;

/* ============================================================================
 * Global State
 * ========================================================================== */

/**
 * @brief Global trace state instance
 *
 * This is the single global instance that holds all tracing state.
 * Access through the API functions rather than directly.
 */
extern nesrt_trace_state_t g_nesrt_trace;

/* ============================================================================
 * Call Stack Tracking
 * ========================================================================== */

/**
 * @brief Push current PC to call stack (call this on JSR)
 *
 * Records the return address (PC after JSR) on the call stack.
 * Also captures the current tag state for later logging.
 *
 * @param pc The return address (PC after JSR instruction)
 *
 * @note Stack overflow is prevented - excess pushes are silently ignored
 *
 * @see nesrt_trace_pop() for the corresponding pop operation
 */
void nesrt_trace_push(uint16_t pc);

/**
 * @brief Pop from call stack (call this on RTS)
 *
 * Removes the top entry from the call stack.
 * Safe to call even if stack is empty (no-op).
 *
 * @see nesrt_trace_push() for the corresponding push operation
 */
void nesrt_trace_pop(void);

/**
 * @brief Get current stack depth
 * @return Number of entries on the call stack (0 = empty)
 */
int nesrt_trace_depth(void);

/**
 * @brief Get max stack depth setting
 * @return Maximum depth to log (default: 5)
 */
int nesrt_trace_get_max_depth(void);

/**
 * @brief Get PC at stack position
 * @param depth Stack position (0 = top/most recent, increasing = older)
 * @return PC value at that depth, or 0 if depth >= current depth
 *
 * @note Depth 0 returns the most recent call, depth 1 returns the caller, etc.
 */
uint16_t nesrt_trace_get_pc(int depth);

/* ============================================================================
 * Tagging System
 * ========================================================================== */

/**
 * @brief Set a tag (max 8 tags active at once)
 *
 * Tags are used to categorize trace events and can be used for filtering.
 * Tags are semicolon-separated in output.
 *
 * @param tag Tag string to add (truncated to NESRT_MAX_TAG_LEN - 1)
 *
 * @note If 8 tags already exist, the new tag is ignored
 * @note Duplicate tags are not checked - caller should avoid
 *
 * @see nesrt_clear_tag() to remove a specific tag
 * @see nesrt_clear_all_tags() to remove all tags
 */
void nesrt_set_tag(const char* tag);

/**
 * @brief Clear a tag
 * @param tag Tag string to remove (case-sensitive match)
 *
 * @note If tag is not found, this is a no-op
 */
void nesrt_clear_tag(const char* tag);

/**
 * @brief Clear all tags
 *
 * Resets the tag list to empty.
 */
void nesrt_clear_all_tags(void);

/**
 * @brief Get tags as semicolon-separated string
 * @return Pointer to internal buffer containing tags (e.g., "tag1;tag2;")
 *
 * @note Returns empty string if no tags are set
 * @note Buffer is owned by the trace system - do not free or modify
 */
const char* nesrt_get_tags(void);

/* ============================================================================
 * Event Logging
 * ========================================================================== */

/**
 * @brief Log a PPU write event with call stack
 * @param addr PPU address being written to ($2000-$2007 or mirrors)
 * @param value Byte value being written
 * @param stack_depth Depth of call stack to include in log
 *
 * @note Respects tag filter - only logs if current tags match filter
 * @note Output format: [tags] PPU_WRITE: $ADDR = $XX (call stack)
 */
void nesrt_log_ppu_write(uint16_t addr, uint8_t value, int stack_depth);

/**
 * @brief Log a PPU read event with call stack
 * @param addr PPU address being read from ($2000-$2007 or mirrors)
 * @param value Byte value being read
 * @param stack_depth Depth of call stack to include in log
 *
 * @note Respects tag filter - only logs if current tags match filter
 * @note Output format: [tags] PPU_READ: $ADDR = $XX (call stack)
 */
void nesrt_log_ppu_read(uint16_t addr, uint8_t value, int stack_depth);

/**
 * @brief Log a controller read event with call stack
 * @param port Controller port (0 = player 1, 1 = player 2)
 * @param value Byte value read from controller
 * @param stack_depth Depth of call stack to include in log
 *
 * @note Respects tag filter - only logs if current tags match filter
 * @note Output format: [tags] CONTROLLER_READ: port=N = $XX (call stack)
 */
void nesrt_log_controller_read(uint8_t port, uint8_t value, int stack_depth);

/**
 * @brief Log a CHR/bank switch event with call stack
 * @param mapper Mapper number
 * @param bank New bank number
 * @param addr Address where bank switch was triggered (write address)
 * @param stack_depth Depth of call stack to include in log
 *
 * @note Respects tag filter - only logs if current tags match filter
 * @note Output format: [tags] BANK_SWITCH: mapper=N bank=N @ $ADDR (call stack)
 */
void nesrt_log_bank_switch(uint8_t mapper, uint8_t bank, uint16_t addr, int stack_depth);

/**
 * @brief Log a custom event with call stack
 * @param event_type Event type string (e.g., "ppustatus", "dma", etc.)
 * @param format Printf-style format string
 * @param ... Format arguments
 *
 * @note Respects tag filter - only logs if current tags match filter
 * @note Output format: [tags] EVENT_TYPE: description (call stack)
 *
 * @see NESRT_LOG_EVENT for a macro version
 */
void nesrt_log_event(const char* event_type, const char* format, ...);

/* ============================================================================
 * Configuration
 * ========================================================================== */

/**
 * @brief Enable/disable tracing
 * @param enable Non-zero to enable, zero to disable
 *
 * @note When disabled, all trace functions return immediately (minimal overhead)
 * @note Call stack is preserved when disabled - re-enabling shows current state
 */
void nesrt_trace_enable(int enable);

/**
 * @brief Set trace output file
 * @param filename Path to output file, or NULL to use stderr
 *
 * @note File is opened in append mode
 * @note If file cannot be opened, falls back to stderr with warning
 * @note Previous file (if any) is closed
 */
void nesrt_trace_set_file(const char* filename);

/**
 * @brief Set max stack depth to log
 * @param depth Maximum depth (1 to NESRT_MAX_STACK_DEPTH, default: 5)
 *
 * @note Values outside valid range are clamped
 * @note This limits how much of the call stack is printed, not tracked
 */
void nesrt_trace_set_max_depth(int depth);

/**
 * @brief Filter by tag (only log events with matching tag)
 * @param tag Tag to filter on, or NULL/empty to disable filtering
 *
 * @note Empty string or NULL disables filtering (all events logged)
 * @note Filter matches if ANY active tag equals the filter tag
 * @note Filter is case-sensitive
 */
void nesrt_trace_set_tag_filter(const char* tag);

/* ============================================================================
 * Inline Helper Functions
 * ========================================================================== */

/**
 * @brief Check if tracing is enabled
 * @return Non-zero if enabled, zero if disabled
 *
 * Use this for quick checks before expensive trace operations.
 */
static inline int nesrt_trace_is_enabled(void) {
    return g_nesrt_trace.enabled;
}

/**
 * @brief Get the current output file
 * @return FILE* for output (stderr if NULL)
 *
 * Useful for writing custom trace messages.
 */
static inline FILE* nesrt_trace_get_file(void) {
    return g_nesrt_trace.output_file ? g_nesrt_trace.output_file : stderr;
}

/**
 * @brief Write the call stack to output
 * @param depth Depth of stack to print (0 = none, -1 = use max_depth)
 *
 * Internal helper for formatting call stacks in logs.
 * Format: "(depth=N: $PC1 -> $PC2 -> ...)"
 */
void nesrt_trace_print_stack(int depth);

/**
 * @brief Check if current tags match the filter
 * @return Non-zero if tags match (or no filter), zero if filtered out
 *
 * Internal helper for tag filtering.
 */
int nesrt_trace_check_tag_filter(void);

/* ============================================================================
 * Convenience Macros
 * ========================================================================== */

/**
 * @brief Log a custom message with call stack
 * @param type Event type string (e.g., "CUSTOM")
 * @param fmt Printf-style format string
 * @param ... Format arguments
 *
 * Example:
 * @code
 * NESRT_LOG_EVENT("DEBUG", "value = %d", some_value);
 * @endcode
 */
#define NESRT_LOG_EVENT(type, fmt, ...) \
    do { \
        if (nesrt_trace_is_enabled() && nesrt_trace_check_tag_filter()) { \
            FILE* _out = nesrt_trace_get_file(); \
            const char* _tags = nesrt_get_tags(); \
            fprintf(_out, "[%s] %s: " fmt, _tags, type, ##__VA_ARGS__); \
            nesrt_trace_print_stack(g_nesrt_trace.max_depth); \
            fprintf(_out, "\n"); \
            fflush(_out); \
        } \
    } while(0)

/**
 * @brief Push and log a call in one operation
 * @param pc Return address to push
 * @param name Optional function name for logging (or NULL)
 *
 * Convenience macro for instrumenting JSR instructions.
 */
#define NESRT_TRACE_CALL(pc, name) \
    do { \
        nesrt_trace_push(pc); \
        if (nesrt_trace_is_enabled() && nesrt_trace_check_tag_filter()) { \
            FILE* _out = nesrt_trace_get_file(); \
            const char* _tags = nesrt_get_tags(); \
            if (name) { \
                fprintf(_out, "[%s] CALL: %s @ $%04X (depth=%d)\n", \
                        _tags, name, (pc), nesrt_trace_depth() - 1); \
            } else { \
                fprintf(_out, "[%s] CALL: $%04X (depth=%d)\n", \
                        _tags, (pc), nesrt_trace_depth() - 1); \
            } \
            fflush(_out); \
        } \
    } while(0)

/**
 * @brief Pop and log a return in one operation
 * @param name Optional function name for logging (or NULL)
 *
 * Convenience macro for instrumenting RTS instructions.
 */
#define NESRT_TRACE_RETURN(name) \
    do { \
        if (nesrt_trace_is_enabled() && nesrt_trace_check_tag_filter()) { \
            FILE* _out = nesrt_trace_get_file(); \
            const char* _tags = nesrt_get_tags(); \
            uint16_t _ret_pc = nesrt_trace_get_pc(0); \
            if (name) { \
                fprintf(_out, "[%s] RETURN: %s -> $%04X\n", _tags, name, _ret_pc); \
            } else { \
                fprintf(_out, "[%s] RETURN -> $%04X\n", _tags, _ret_pc); \
            } \
            fflush(_out); \
        } \
        nesrt_trace_pop(); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* NESRT_TRACE_H */
