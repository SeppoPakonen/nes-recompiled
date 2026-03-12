/**
 * @file platform_sdl.cpp
 * @brief SDL2 platform implementation for NES runtime with ImGui
 */

#include "platform_sdl.h"
#include "nesrt.h"
#include "ppu.h"
#include "audio_stats.h"
#include "nesrt_debug.h"

#ifdef NES_HAS_SDL2
#include <SDL.h>
#include <atomic>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

/* ============================================================================
 * SDL State
 * ========================================================================== */

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static int g_scale = 3;
static uint32_t g_last_frame_time = 0;
static SDL_AudioDeviceID g_audio_device = 0;
static SDL_GameController* g_controller = NULL;
static bool g_vsync = false;  /* VSync OFF - we pace with wall clock for 60.0 FPS */
static bool g_audio_started = false;
static uint32_t g_audio_start_threshold = 0;

/* Performance timing diagnostics */
static uint32_t g_timing_render_total = 0;
static uint32_t g_timing_vsync_total = 0;
static uint32_t g_timing_frame_count = 0;

/* Menu State */
static bool g_show_menu = false;
static int g_speed_percent = 100;
static const char* g_scale_names[] = { "1x (256x240)", "2x (512x480)", "3x (768x720)", "4x (1024x960)", "5x (1280x1200)", "6x (1536x1440)", "7x (1792x1680)", "8x (2048x1920)" };

/* Joypad state - NES controller: A, B, Select, Start, D-Pad */
/* Active low: bits correspond to A, B, Select, Start, Up, Down, Left, Right */
uint8_t g_joypad_buttons = 0xFF;  /* Active low: A=bit0, B=bit1, Select=bit2, Start=bit3 */
uint8_t g_joypad_dpad = 0xFF;     /* Active low: Down=bit4, Up=bit5, Left=bit6, Right=bit7 */

/* ============================================================================
 * Automation State
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SCRIPT_ENTRIES 100
typedef struct {
    uint32_t start_frame;
    uint32_t duration;
    uint8_t dpad;    /* Active LOW mask to apply (0 = Pressed) */
    uint8_t buttons; /* Active LOW mask to apply (0 = Pressed) */
} ScriptEntry;

static ScriptEntry g_input_script[MAX_SCRIPT_ENTRIES];
static int g_script_count = 0;

#define MAX_DUMP_FRAMES 100
static uint32_t g_dump_frames[MAX_DUMP_FRAMES];
static int g_dump_count = 0;
static char g_screenshot_prefix[64] = "screenshot";

/* Helper to parse button string for NES: "U,D,L,R,A,B,S,T" */
static void parse_buttons(const char* btn_str, uint8_t* dpad, uint8_t* buttons) {
    *dpad = 0xFF;
    *buttons = 0xFF;
    /* NES button mapping: A, B, Select, Start, D-Pad */
    if (strchr(btn_str, 'U')) *dpad &= ~0x20;  /* Up = bit 5 */
    if (strchr(btn_str, 'D')) *dpad &= ~0x10;  /* Down = bit 4 */
    if (strchr(btn_str, 'L')) *dpad &= ~0x40;  /* Left = bit 6 */
    if (strchr(btn_str, 'R')) *dpad &= ~0x80;  /* Right = bit 7 */
    if (strchr(btn_str, 'A')) *buttons &= ~0x01;  /* A = bit 0 */
    if (strchr(btn_str, 'B')) *buttons &= ~0x02;  /* B = bit 1 */
    if (strchr(btn_str, 'S')) *buttons &= ~0x08;  /* Start = bit 3 */
    if (strchr(btn_str, 'T')) *buttons &= ~0x04;  /* Select = bit 2 (T for selecT) */
}

void nes_platform_set_input_script(const char* script) {
    /* Format: frame:buttons:duration,... */
    if (!script) return;

    char* copy = strdup(script);
    char* token = strtok(copy, ",");
    g_script_count = 0;

    while (token && g_script_count < MAX_SCRIPT_ENTRIES) {
        uint32_t frame = 0, duration = 0;
        char btn_buf[16] = {0};

        if (sscanf(token, "%u:%15[^:]:%u", &frame, btn_buf, &duration) == 3) {
            ScriptEntry* e = &g_input_script[g_script_count++];
            e->start_frame = frame;
            e->duration = duration;
            parse_buttons(btn_buf, &e->dpad, &e->buttons);
            printf("[AUTO] Added input: Frame %u, Btns '%s', Dur %u\n", frame, btn_buf, duration);
        }
        token = strtok(NULL, ",");
    }
    free(copy);
}

void nes_platform_set_dump_frames(const char* frames) {
    if (!frames) return;
    char* copy = strdup(frames);
    char* token = strtok(copy, ",");
    g_dump_count = 0;
    while (token && g_dump_count < MAX_DUMP_FRAMES) {
        g_dump_frames[g_dump_count++] = (uint32_t)strtoul(token, NULL, 10);
        token = strtok(NULL, ",");
    }
    free(copy);
}

void nes_platform_set_screenshot_prefix(const char* prefix) {
    if (prefix) snprintf(g_screenshot_prefix, sizeof(g_screenshot_prefix), "%s", prefix);
}

static void save_ppm(const char* filename, const uint32_t* fb, int width, int height, int frame_count) {
    /* Calculate simple hash */
    uint32_t hash = 0;
    for (int k = 0; k < width * height; k++) {
        hash = (hash * 33) ^ fb[k];
    }
    printf("[AUTO] Frame %d hash: %08X\n", frame_count, hash);

    FILE* f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", width, height);

    uint8_t* row = (uint8_t*)malloc(width * 3);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t p = fb[y * width + x];
            row[x*3+0] = (p >> 16) & 0xFF; /* R */
            row[x*3+1] = (p >> 8) & 0xFF;  /* G */
            row[x*3+2] = (p >> 0) & 0xFF;  /* B */
        }
        fwrite(row, 1, width * 3, f);
    }

    free(row);
    fclose(f);
    printf("[AUTO] Saved screenshot: %s\n", filename);
}


static int g_frame_count = 0;

/* ============================================================================
 * Platform Functions
 * ========================================================================== */

void nes_platform_shutdown(void) {
    if (g_audio_device) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }

    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
    g_audio_started = false;
    g_audio_start_threshold = 0;

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = NULL;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    SDL_Quit();
}

/* ============================================================================
 * Audio - Simple Push Mode with SDL_QueueAudio
 * ========================================================================== */

#define AUDIO_SAMPLE_RATE 44100

/*
 * Audio: Simple circular buffer with SDL callback
 * The callback pulls samples at exactly 44100 Hz.
 * The emulator pushes samples as they're generated.
 * A large buffer provides tolerance for timing variations.
 */
#define AUDIO_RING_SIZE 16384  /* ~370ms buffer - plenty of headroom */
static int16_t g_audio_ring[AUDIO_RING_SIZE * 2];  /* Stereo */
static std::atomic<uint32_t> g_audio_write_pos{0};
static std::atomic<uint32_t> g_audio_read_pos{0};
static uint32_t g_audio_device_sample_rate = AUDIO_SAMPLE_RATE;

/* Debug counters */
static uint32_t g_audio_samples_written = 0;
static uint32_t g_audio_underruns = 0;

static uint32_t audio_ring_fill_samples(void) {
    uint32_t write_pos = g_audio_write_pos.load(std::memory_order_acquire);
    uint32_t read_pos = g_audio_read_pos.load(std::memory_order_acquire);
    return (write_pos >= read_pos) ? (write_pos - read_pos) : (AUDIO_RING_SIZE - read_pos + write_pos);
}

static void update_audio_stats_from_ring(void) {
    audio_stats_update_buffer(audio_ring_fill_samples(), AUDIO_RING_SIZE, g_audio_device_sample_rate);
}

/* SDL callback - pulls samples from ring buffer */
static void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    int16_t* out = (int16_t*)stream;
    int samples_needed = len / 4;  /* Stereo 16-bit = 4 bytes per sample */

    uint32_t write_pos = g_audio_write_pos.load(std::memory_order_acquire);
    uint32_t read_pos = g_audio_read_pos.load(std::memory_order_relaxed);

    for (int i = 0; i < samples_needed; i++) {
        if (read_pos != write_pos) {
            /* Have data - copy it */
            out[i * 2] = g_audio_ring[read_pos * 2];
            out[i * 2 + 1] = g_audio_ring[read_pos * 2 + 1];
            read_pos = (read_pos + 1) % AUDIO_RING_SIZE;
        } else {
            /* Underrun - output silence */
            out[i * 2] = 0;
            out[i * 2 + 1] = 0;
            g_audio_underruns++;
            audio_stats_underrun();
        }
    }

    g_audio_read_pos.store(read_pos, std::memory_order_release);
}

static void on_audio_sample(NESContext* ctx, int16_t left, int16_t right) {
    (void)ctx;
    if (g_audio_device == 0) return;

    uint32_t write_pos = g_audio_write_pos.load(std::memory_order_relaxed);
    uint32_t next_write = (write_pos + 1) % AUDIO_RING_SIZE;

    /* If buffer is full, drop this sample (prevents blocking) */
    if (next_write == g_audio_read_pos.load(std::memory_order_acquire)) {
        audio_stats_samples_dropped(1);
        return;  /* Drop sample */
    }

    g_audio_ring[write_pos * 2] = left;
    g_audio_ring[write_pos * 2 + 1] = right;
    g_audio_write_pos.store(next_write, std::memory_order_release);
    g_audio_samples_written++;
    audio_stats_samples_queued(1);

    if (!g_audio_started && audio_ring_fill_samples() >= g_audio_start_threshold) {
        SDL_PauseAudioDevice(g_audio_device, 0);
        g_audio_started = true;
    }
}

bool nes_platform_init(int scale) {
    g_scale = scale;
    if (g_scale < 1) g_scale = 1;
    if (g_scale > 8) g_scale = 8;

    fprintf(stderr, "[SDL] Initializing SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "[SDL] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    fprintf(stderr, "[SDL] SDL initialized.\n");

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                fprintf(stderr, "[SDL] Controller: %s\n", SDL_GameControllerName(g_controller));
                break;
            }
        }
    }

    /* Initialize Audio - Callback Mode with large buffer */
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 2048;  /* Larger buffer for smooth playback */
    want.callback = sdl_audio_callback;
    want.userdata = NULL;

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_audio_device == 0) {
        fprintf(stderr, "[SDL] Failed to open audio: %s\n", SDL_GetError());
    } else {
        g_audio_write_pos.store(0, std::memory_order_relaxed);
        g_audio_read_pos.store(0, std::memory_order_relaxed);
        g_audio_started = false;
        g_audio_device_sample_rate = (uint32_t)have.freq;
        g_audio_start_threshold = (uint32_t)have.samples;
        if (g_audio_start_threshold == 0) g_audio_start_threshold = 1;
        if (g_audio_start_threshold > AUDIO_RING_SIZE / 2) {
            g_audio_start_threshold = AUDIO_RING_SIZE / 2;
        }
        fprintf(stderr, "[SDL] Audio initialized: %d Hz, %d channels, buffer %d samples (Callback Mode)\n",
                have.freq, have.channels, have.samples);
        update_audio_stats_from_ring();
    }

    fprintf(stderr, "[SDL] Creating window...\n");
    g_window = SDL_CreateWindow(
        "NES Recompiled",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        NES_SCREEN_WIDTH * g_scale,
        NES_SCREEN_HEIGHT * g_scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!g_window) {
        fprintf(stderr, "[SDL] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    fprintf(stderr, "[SDL] Window created.\n");

    fprintf(stderr, "[SDL] Creating renderer...\n");
    /*
     * NO VSync - we use wall-clock timing to run at exactly 60.0 FPS.
     * This is essential for non-60Hz monitors (like 100Hz).
     */
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);

    if (!g_renderer) {
        fprintf(stderr, "[SDL] Hardware renderer failed, trying software fallback...\n");
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!g_renderer) {
        fprintf(stderr, "[SDL] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    /* Setup Dear ImGui context */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     /* Enable Keyboard Controls */
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      /* Enable Gamepad Controls */

    /* Setup Dear ImGui style */
    ImGui::StyleColorsDark();

    /* Setup Platform/Renderer backends */
    ImGui_ImplSDL2_InitForSDLRenderer(g_window, g_renderer);
    ImGui_ImplSDLRenderer2_Init(g_renderer);

    g_texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        NES_SCREEN_WIDTH,
        NES_SCREEN_HEIGHT
    );

    if (!g_texture) {
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return false;
    }

    g_last_frame_time = SDL_GetTicks();

    return true;
}

void nes_platform_register_context(NESContext* ctx) {
    /* Set up platform callbacks */
    if (ctx) {
        ctx->callbacks.on_audio_sample = on_audio_sample;
        ctx->callbacks.get_joypad = NULL;  /* We handle joypad directly */
        ctx->callbacks.on_vblank = NULL;
        ctx->callbacks.on_serial_byte = NULL;
        ctx->callbacks.load_battery_ram = NULL;
        ctx->callbacks.save_battery_ram = NULL;
    }
}

bool nes_platform_poll_events(NESContext* ctx) {
    (void)ctx;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
         ImGui_ImplSDL2_ProcessEvent(&event);
         if (event.type == SDL_QUIT) return false;
         if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(g_window))
            return false;

        switch (event.type) {
            case SDL_CONTROLLERAXISMOTION: {
                const int deadzone = 8000;

                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                    if (event.caxis.value > deadzone) {
                        g_joypad_dpad &= ~0x80;  /* Right */
                        g_joypad_dpad |= 0x40;
                    } else if (event.caxis.value < -deadzone) {
                        g_joypad_dpad &= ~0x40;  /* Left */
                        g_joypad_dpad |= 0x80;
                    } else {
                        g_joypad_dpad |= 0x40;
                        g_joypad_dpad |= 0x80;
                    }
                }

                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                    if (event.caxis.value > deadzone) {
                        g_joypad_dpad &= ~0x10;  /* Down */
                        g_joypad_dpad |= 0x20;
                    } else if (event.caxis.value < -deadzone) {
                        g_joypad_dpad &= ~0x20;  /* Up */
                        g_joypad_dpad |= 0x10;
                    } else {
                        g_joypad_dpad |= 0x10;
                        g_joypad_dpad |= 0x20;
                    }
                }

                break;
            }

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);

                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        if (pressed) g_joypad_dpad &= ~0x20;
                        else g_joypad_dpad |= 0x20;
                        break;

                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        if (pressed) g_joypad_dpad &= ~0x10;
                        else g_joypad_dpad |= 0x10;
                        break;

                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        if (pressed) g_joypad_dpad &= ~0x40;
                        else g_joypad_dpad |= 0x40;
                        break;

                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        if (pressed) g_joypad_dpad &= ~0x80;
                        else g_joypad_dpad |= 0x80;
                        break;

                    case SDL_CONTROLLER_BUTTON_A:
                        /* Map to NES A button */
                        if (pressed) g_joypad_buttons &= ~0x01;
                        else g_joypad_buttons |= 0x01;
                        break;

                    case SDL_CONTROLLER_BUTTON_B:
                        /* Map to NES B button */
                        if (pressed) g_joypad_buttons &= ~0x02;
                        else g_joypad_buttons |= 0x02;
                        break;

                    case SDL_CONTROLLER_BUTTON_START:
                        /* Map to NES Start */
                        if (pressed) g_joypad_buttons &= ~0x08;
                        else g_joypad_buttons |= 0x08;
                        break;

                    case SDL_CONTROLLER_BUTTON_BACK:
                        /* Map to NES Select */
                        if (pressed) g_joypad_buttons &= ~0x04;
                        else g_joypad_buttons |= 0x04;
                        break;
                        
                    case SDL_CONTROLLER_BUTTON_X:
                        /* Also map X to NES A (alternative) */
                        if (pressed) g_joypad_buttons &= ~0x01;
                        else g_joypad_buttons |= 0x01;
                        break;
                        
                    case SDL_CONTROLLER_BUTTON_Y:
                        /* Also map Y to NES B (alternative) */
                        if (pressed) g_joypad_buttons &= ~0x02;
                        else g_joypad_buttons |= 0x02;
                        break;
                }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool pressed = (event.type == SDL_KEYDOWN);

                switch (event.key.keysym.scancode) {
                    /* D-pad */
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_W:
                        if (pressed) g_joypad_dpad &= ~0x20;
                        else g_joypad_dpad |= 0x20;
                        break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_S:
                        if (pressed) g_joypad_dpad &= ~0x10;
                        else g_joypad_dpad |= 0x10;
                        break;
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_A:
                        if (pressed) g_joypad_dpad &= ~0x40;
                        else g_joypad_dpad |= 0x40;
                        break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_D:
                        if (pressed) g_joypad_dpad &= ~0x80;
                        else g_joypad_dpad |= 0x80;
                        break;

                    /* Buttons - NES layout */
                    case SDL_SCANCODE_J:
                    case SDL_SCANCODE_K:
                        /* A button */
                        if (pressed) g_joypad_buttons &= ~0x01;
                        else g_joypad_buttons |= 0x01;
                        break;
                    case SDL_SCANCODE_I:
                    case SDL_SCANCODE_L:
                        /* B button */
                        if (pressed) g_joypad_buttons &= ~0x02;
                        else g_joypad_buttons |= 0x02;
                        break;
                    case SDL_SCANCODE_RSHIFT:
                    case SDL_SCANCODE_BACKSPACE:
                        /* Select */
                        if (pressed) g_joypad_buttons &= ~0x04;
                        else g_joypad_buttons |= 0x04;
                        break;
                    case SDL_SCANCODE_RETURN:
                        /* Start */
                        if (pressed) g_joypad_buttons &= ~0x08;
                        else g_joypad_buttons |= 0x08;
                        break;

                    case SDL_SCANCODE_ESCAPE:
                        if (pressed) {
                            g_show_menu = !g_show_menu;
                        }
                        return true; /* Don't block */

                    default:
                        break;
                }
                break;
            }

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    /* Handle resize if needed or just let SDL/ImGui handle it */
                }
                break;
        }
    }

    /* Handle Automation Inputs */
    for (int i = 0; i < g_script_count; i++) {
        ScriptEntry* e = &g_input_script[i];
        if (g_frame_count >= (int)e->start_frame && g_frame_count < (int)(e->start_frame + e->duration)) {
             /* Apply inputs (ANDing masks) */
             g_joypad_dpad &= e->dpad;
             g_joypad_buttons &= e->buttons;
        }
    }

    return true;
}

void nes_platform_render_frame(const uint32_t* framebuffer) {
    if (!g_texture || !g_renderer || !framebuffer) {
        DBG_FRAME("Platform render_frame: SKIPPED (null: texture=%d, renderer=%d, fb=%d)",
                  g_texture == NULL, g_renderer == NULL, framebuffer == NULL);
        return;
    }

    g_frame_count++;

    /* Handle Screenshot Dumping */
    for (int i = 0; i < g_dump_count; i++) {
        if (g_dump_frames[i] == (uint32_t)g_frame_count) {
             char filename[128];
             snprintf(filename, sizeof(filename), "%s_%05d.ppm", g_screenshot_prefix, g_frame_count);
             save_ppm(filename, framebuffer, NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT, g_frame_count);
        }
    }

    if (g_frame_count % 60 == 0) {
        char title[64];
        snprintf(title, sizeof(title), "NES Recompiled - Frame %d", g_frame_count);
        SDL_SetWindowTitle(g_window, title);
    }

    /* Update texture */
    void* pixels;
    int pitch;
    SDL_LockTexture(g_texture, NULL, &pixels, &pitch);

    /* Copy framebuffer directly (NES PPU outputs RGB) */
    memcpy(pixels, framebuffer, NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT * sizeof(uint32_t));

    SDL_UnlockTexture(g_texture);

    /* Clear and render */
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);

    /* ImGui Frame */
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (g_show_menu) {
        ImGui::Begin("NES Recompiled", &g_show_menu);
        ImGui::Text("Performance: %.1f FPS", ImGui::GetIO().Framerate);
        int scale_idx = g_scale - 1;
        if (ImGui::Combo("Resolution", &scale_idx, g_scale_names, IM_ARRAYSIZE(g_scale_names))) {
            g_scale = scale_idx + 1;
            SDL_SetWindowSize(g_window, NES_SCREEN_WIDTH * g_scale, NES_SCREEN_HEIGHT * g_scale);
            SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }

        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::BulletText("D-Pad: Arrow Keys / WASD");
        ImGui::BulletText("A: J / K");
        ImGui::BulletText("B: I / L");
        ImGui::BulletText("Select: Right Shift / Backspace");
        ImGui::BulletText("Start: Enter");
        ImGui::BulletText("Menu: Escape");

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(g_renderer);
}

uint8_t nes_platform_get_joypad(void) {
    return g_joypad_buttons;
}

void nes_platform_vsync(void) {
    /* Wall-clock frame pacing for 60.0 FPS */
    uint32_t target_frame_time = 1000 / 60;  /* ~16.67ms per frame */
    uint32_t current_time = SDL_GetTicks();
    uint32_t elapsed = current_time - g_last_frame_time;

    if (elapsed < target_frame_time) {
        uint32_t delay = target_frame_time - elapsed;
        SDL_Delay(delay);
    }

    g_last_frame_time = SDL_GetTicks();
}

void nes_platform_set_title(const char* title) {
    if (g_window && title) {
        SDL_SetWindowTitle(g_window, title);
    }
}

#endif /* NES_HAS_SDL2 */
