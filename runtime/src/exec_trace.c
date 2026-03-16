/**
 * @file exec_trace.c
 * @brief Execution tracing for debugging game flow
 */

#include "nesrt.h"
#include <stdio.h>

/* Execution tracing state */
static bool g_exec_trace_enabled = false;
static FILE* g_exec_trace_file = NULL;
static uint32_t g_exec_trace_count = 0;

void nes_exec_trace_init(void) {
    g_exec_trace_enabled = false;
    g_exec_trace_file = NULL;
    g_exec_trace_count = 0;
}

void nes_exec_trace_enable(int enable) {
    g_exec_trace_enabled = (enable != 0);
}

void nes_exec_trace_log(const char* func_name, uint16_t addr, uint8_t bank) {
    if (!g_exec_trace_enabled) return;
    
    if (!g_exec_trace_file) {
        g_exec_trace_file = fopen("logs/exec_trace.log", "w");
        if (!g_exec_trace_file) return;
    }
    
    fprintf(g_exec_trace_file, "[%u] func_%02x_%04x: %s\n", 
            g_exec_trace_count++, (int)bank, (int)addr, func_name);
    fflush(g_exec_trace_file);
}

void nes_exec_trace_close(void) {
    if (g_exec_trace_file) {
        fclose(g_exec_trace_file);
        g_exec_trace_file = NULL;
    }
}
