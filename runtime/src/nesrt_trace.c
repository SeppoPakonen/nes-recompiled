/**
 * @file nesrt_trace.c
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

#include "nesrt_trace.h"
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * Global State
 * ========================================================================== */

/**
 * @brief Global trace state instance
 *
 * This is the single global instance that holds all tracing state.
 * All tracing functions operate on this state.
 */
nesrt_trace_state_t g_nesrt_trace = {0};

/**
 * @brief Output buffer for formatted strings
 *
 * Used for building formatted output before writing to file/stderr.
 * This avoids repeated small writes and allows for atomic output.
 */
static char g_output_buffer[4096];

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief Rebuild tags_str from tags array
 *
 * Creates a semicolon-separated string from all active tags.
 * This is called whenever tags are modified.
 *
 * @note Internal function - not part of public API
 */
static void rebuild_tags_str(void) {
    g_nesrt_trace.tags_str[0] = '\0';
    
    size_t pos = 0;
    for (int i = 0; i < NESRT_MAX_TAGS; i++) {
        if (g_nesrt_trace.tags[i][0] != '\0') {
            size_t tag_len = strlen(g_nesrt_trace.tags[i]);
            
            // Check if we have room for tag + semicolon + null terminator
            if (pos + tag_len + 1 < NESRT_MAX_TAGS_STR) {
                memcpy(g_nesrt_trace.tags_str + pos, g_nesrt_trace.tags[i], tag_len);
                pos += tag_len;
                g_nesrt_trace.tags_str[pos++] = ';';
            }
        }
    }
    
    // Remove trailing semicolon if present
    if (pos > 0 && g_nesrt_trace.tags_str[pos - 1] == ';') {
        g_nesrt_trace.tags_str[pos - 1] = '\0';
    } else {
        g_nesrt_trace.tags_str[pos] = '\0';
    }
}

/**
 * @brief Check if current tags match the filter
 * @return Non-zero if tags match (or no filter), zero if filtered out
 *
 * If no filter is set, always returns true.
 * If filter is set, returns true if ANY active tag matches the filter.
 *
 * @note Internal function - not part of public API
 */
static int tags_match_filter(void) {
    // No filter = match everything
    if (g_nesrt_trace.tag_filter[0] == '\0') {
        return 1;
    }

    // Check if any active tag contains the filter string (substring match)
    for (int i = 0; i < NESRT_MAX_TAGS; i++) {
        if (g_nesrt_trace.tags[i][0] != '\0') {
            if (strstr(g_nesrt_trace.tags[i], g_nesrt_trace.tag_filter) != NULL) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Format call stack to buffer
 * @param buffer Output buffer
 * @param size Buffer size
 * @param max_depth Maximum depth to include (0 = none, -1 = use max_depth)
 *
 * Formats the call stack as: "(depth=N: $PC1 -> $PC2 -> ...)"
 * Shows most recent calls first (top of stack).
 *
 * @note Internal function - not part of public API
 */
static void format_call_stack(char* buffer, size_t size, int max_depth) {
    if (max_depth < 0) {
        max_depth = g_nesrt_trace.max_depth;
    }
    
    // Clamp max_depth to valid range
    if (max_depth > g_nesrt_trace.stack_depth) {
        max_depth = g_nesrt_trace.stack_depth;
    }
    if (max_depth > NESRT_MAX_STACK_DEPTH) {
        max_depth = NESRT_MAX_STACK_DEPTH;
    }
    
    int written = 0;
    
    // Start with depth indicator
    written = snprintf(buffer, size, "(depth=%d", g_nesrt_trace.stack_depth);
    
    if (max_depth > 0) {
        written += snprintf(buffer + written, size - written, ": ");
        
        // Show stack from most recent (top) to oldest
        // Stack grows upward, so top is at stack_depth - 1
        for (int i = 0; i < max_depth; i++) {
            int stack_idx = g_nesrt_trace.stack_depth - 1 - i;
            if (stack_idx >= 0) {
                if (i > 0) {
                    written += snprintf(buffer + written, size - written, " -> ");
                }
                written += snprintf(buffer + written, size - written, "$%04X",
                                   g_nesrt_trace.stack[stack_idx].pc);
            }
        }
    }
    
    snprintf(buffer + written, size - written, ")");
}

/**
 * @brief Log generic event with formatting
 * @param event_type Type string for the event (e.g., "PPU_WRITE")
 * @param format Printf-style format string for event details
 * @param args Variadic arguments for format string
 *
 * This is the core logging function used by all specific event loggers.
 * It handles tag formatting, call stack output, and file writing.
 *
 * @note Internal function - not part of public API
 */
static void log_event_v(const char* event_type, const char* format, va_list args) {
    // Check if tracing is enabled
    if (!g_nesrt_trace.enabled) {
        return;
    }

    // Check tag filter
    if (!tags_match_filter()) {
        return;
    }

    // Get output file
    FILE* out = g_nesrt_trace.output_file ? g_nesrt_trace.output_file : stderr;
    
    // Format the event header with tags
    int written = snprintf(g_output_buffer, sizeof(g_output_buffer),
                          "[%s] %s: ", g_nesrt_trace.tags_str, event_type);
    
    // Format the event details
    written += vsnprintf(g_output_buffer + written, sizeof(g_output_buffer) - written,
                        format, args);
    
    // Write the formatted event
    fputs(g_output_buffer, out);
    
    // Format and write call stack
    format_call_stack(g_output_buffer, sizeof(g_output_buffer), -1);
    fputs(" ", out);
    fputs(g_output_buffer, out);
    
    // Finish with newline
    fputc('\n', out);
    fflush(out);
}

/**
 * @brief Log generic event (variadic wrapper)
 * @param event_type Type string for the event
 * @param format Printf-style format string
 * @param ... Format arguments
 */
static void log_event(const char* event_type, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_event_v(event_type, format, args);
    va_end(args);
}

/* ============================================================================
 * Call Stack Tracking Implementation
 * ========================================================================== */

void nesrt_trace_push(uint16_t pc) {
    if (g_nesrt_trace.stack_depth >= NESRT_MAX_STACK_DEPTH) {
        // Stack overflow - silently ignore to prevent corruption
        // This could happen with very deep recursion or broken code
        return;
    }
    
    // Push PC and current tag state
    g_nesrt_trace.stack[g_nesrt_trace.stack_depth].pc = pc;
    g_nesrt_trace.stack[g_nesrt_trace.stack_depth].tag_str = g_nesrt_trace.tags_str;
    g_nesrt_trace.stack_depth++;
}

void nesrt_trace_pop(void) {
    if (g_nesrt_trace.stack_depth > 0) {
        g_nesrt_trace.stack_depth--;
    }
    // Underflow is silently ignored - safe to call even on empty stack
}

int nesrt_trace_depth(void) {
    return g_nesrt_trace.stack_depth;
}

int nesrt_trace_get_max_depth(void) {
    return g_nesrt_trace.max_depth;
}

uint16_t nesrt_trace_get_pc(int depth) {
    if (depth < 0 || depth >= g_nesrt_trace.stack_depth) {
        return 0;
    }
    
    // Depth 0 = top of stack (most recent call)
    int stack_idx = g_nesrt_trace.stack_depth - 1 - depth;
    return g_nesrt_trace.stack[stack_idx].pc;
}

/* ============================================================================
 * Tagging System Implementation
 * ========================================================================== */

void nesrt_set_tag(const char* tag) {
    if (!tag || tag[0] == '\0') {
        return;
    }
    
    // Find empty slot or check if already exists
    for (int i = 0; i < NESRT_MAX_TAGS; i++) {
        // Check for duplicate
        if (strcmp(g_nesrt_trace.tags[i], tag) == 0) {
            return;  // Already exists
        }
        
        // Find empty slot
        if (g_nesrt_trace.tags[i][0] == '\0') {
            // Copy tag (truncate if necessary)
            strncpy(g_nesrt_trace.tags[i], tag, NESRT_MAX_TAG_LEN - 1);
            g_nesrt_trace.tags[i][NESRT_MAX_TAG_LEN - 1] = '\0';
            
            // Rebuild the semicolon-separated string
            rebuild_tags_str();
            return;
        }
    }
    
    // No empty slot - all 8 tags are used, silently ignore
}

void nesrt_clear_tag(const char* tag) {
    if (!tag || tag[0] == '\0') {
        return;
    }
    
    for (int i = 0; i < NESRT_MAX_TAGS; i++) {
        if (strcmp(g_nesrt_trace.tags[i], tag) == 0) {
            g_nesrt_trace.tags[i][0] = '\0';
            rebuild_tags_str();
            return;
        }
    }
}

void nesrt_clear_all_tags(void) {
    for (int i = 0; i < NESRT_MAX_TAGS; i++) {
        g_nesrt_trace.tags[i][0] = '\0';
    }
    g_nesrt_trace.tags_str[0] = '\0';
}

const char* nesrt_get_tags(void) {
    return g_nesrt_trace.tags_str;
}

/* ============================================================================
 * Event Logging Implementation
 * ========================================================================== */

void nesrt_log_ppu_write(uint16_t addr, uint8_t value, int stack_depth) {
    (void)stack_depth;  // Stack depth is handled by format_call_stack
    log_event("PPU_WRITE", "$%04X = $%02X", addr, value);
}

void nesrt_log_ppu_read(uint16_t addr, uint8_t value, int stack_depth) {
    (void)stack_depth;  // Stack depth is handled by format_call_stack
    log_event("PPU_READ", "$%04X = $%02X", addr, value);
}

void nesrt_log_controller_read(uint8_t port, uint8_t value, int stack_depth) {
    (void)stack_depth;  // Stack depth is handled by format_call_stack
    log_event("CONTROLLER_READ", "port=%d = $%02X", port, value);
}

void nesrt_log_bank_switch(uint8_t mapper, uint8_t bank, uint16_t addr, int stack_depth) {
    (void)stack_depth;  // Stack depth is handled by format_call_stack
    log_event("BANK_SWITCH", "mapper=%d bank=%d @ $%04X", mapper, bank, addr);
}

void nesrt_log_event(const char* event_type, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_event_v(event_type, format, args);
    va_end(args);
}

/* ============================================================================
 * Configuration Implementation
 * ========================================================================== */

void nesrt_trace_enable(int enable) {
    g_nesrt_trace.enabled = enable ? 1 : 0;
}

void nesrt_trace_set_file(const char* filename) {
    // Close previous file if open
    if (g_nesrt_trace.output_file) {
        fclose(g_nesrt_trace.output_file);
        g_nesrt_trace.output_file = NULL;
    }

    // Open new file if filename provided
    if (filename && filename[0] != '\0') {
        g_nesrt_trace.output_file = fopen(filename, "a");
        if (!g_nesrt_trace.output_file) {
            fprintf(stderr, "[NESRT_TRACE] Warning: Could not open trace file '%s', using stderr\n", filename);
        }
    }
}

void nesrt_trace_set_max_depth(int depth) {
    // Clamp to valid range
    if (depth < 1) {
        depth = 1;
    } else if (depth > NESRT_MAX_STACK_DEPTH) {
        depth = NESRT_MAX_STACK_DEPTH;
    }
    g_nesrt_trace.max_depth = depth;
}

void nesrt_trace_set_tag_filter(const char* tag) {
    if (!tag || tag[0] == '\0') {
        g_nesrt_trace.tag_filter[0] = '\0';
    } else {
        strncpy(g_nesrt_trace.tag_filter, tag, NESRT_MAX_TAG_LEN - 1);
        g_nesrt_trace.tag_filter[NESRT_MAX_TAG_LEN - 1] = '\0';
    }
}

/* ============================================================================
 * Public Helper Functions Implementation
 * ========================================================================== */

void nesrt_trace_print_stack(int depth) {
    if (depth < 0) {
        depth = g_nesrt_trace.max_depth;
    }
    
    format_call_stack(g_output_buffer, sizeof(g_output_buffer), depth);
    
    FILE* out = g_nesrt_trace.output_file ? g_nesrt_trace.output_file : stderr;
    fputs(g_output_buffer, out);
}

int nesrt_trace_check_tag_filter(void) {
    return tags_match_filter();
}

/* ============================================================================
 * Example Usage
 * ========================================================================== */

/**
 * @example Example usage in recompiled code:
 *
 * @code
 * // Initialize tracing at program start
 * void init_tracing(void) {
 *     nesrt_trace_enable(1);
 *     nesrt_trace_set_file("game_trace.log");
 *     nesrt_trace_set_max_depth(5);
 *     
 *     // Set tags for different game phases
 *     nesrt_set_tag("init");
 * }
 *
 * // Track function calls in recompiled code
 * void func_8000(NESContext* ctx) {
 *     NESRT_TRACE_CALL(ctx->pc, "func_8000");
 *     
 *     // Function body...
 *     // The macro handles push and logging
 *     
 *     NESRT_TRACE_RETURN("func_8000");
 *     // The macro handles logging and pop
 * }
 *
 * // Log hardware events from runtime
 * void nes_write8(uint16_t addr, uint8_t value) {
 *     if (addr >= 0x2000 && addr <= 0x2007) {
 *         nesrt_log_ppu_write(addr, value, 3);
 *     }
 *     // ... rest of write handling
 * }
 *
 * // Use tags for filtering specific systems
 * void render_frame(void) {
 *     nesrt_set_tag("rendering");
 *     // ... rendering code that triggers PPU writes ...
 *     nesrt_clear_tag("rendering");
 * }
 *
 * // Filter to only see rendering-related events
 * nesrt_trace_set_tag_filter("rendering");
 * @endcode
 */
