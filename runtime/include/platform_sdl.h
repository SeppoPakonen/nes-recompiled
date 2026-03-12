/**
 * @file platform_sdl.h
 * @brief SDL2 platform layer for NES runtime
 */

#ifndef NES_PLATFORM_SDL_H
#define NES_PLATFORM_SDL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NES screen resolution */
#define NES_SCREEN_WIDTH  256
#define NES_SCREEN_HEIGHT 240

// Joypad state variables
extern uint8_t g_joypad_buttons;
extern uint8_t g_joypad_dpad;

typedef struct NESContext NESContext;

/**
 * @brief Initialize SDL2 platform (window, renderer)
 * @param scale Window scale factor (1-4)
 * @return true on success
 */
bool nes_platform_init(int scale);

/**
 * @brief Register context with platform (sets up callbacks)
 */
void nes_platform_register_context(NESContext* ctx);

/**
 * @brief Shutdown SDL2 platform
 */
void nes_platform_shutdown(void);

/**
 * @brief Process SDL events
 * @return false if quit requested
 */
bool nes_platform_poll_events(NESContext* ctx);

/**
 * @brief Render frame to screen
 */
void nes_platform_render_frame(const uint32_t* framebuffer);

/**
 * @brief Get joypad state
 * @return Joypad byte (active low)
 */
uint8_t nes_platform_get_joypad(void);

/**
 * @brief Wait for vsync / frame timing
 */
void nes_platform_vsync(void);

/**
 * @brief Set window title
 */
void nes_platform_set_title(const char* title);

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

#ifdef __cplusplus
}
#endif

#endif /* NES_PLATFORM_SDL_H */
