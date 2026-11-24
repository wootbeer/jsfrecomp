#ifndef JSF_CONFIG_H
#define JSF_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define JSF_PATH_MAX 1024

/* VSync: On waits for the display (FIFO); Fast shows the newest frame at
 * each refresh without tearing (mailbox); Off shows frames at once (may
 * tear). */
enum { JSF_VSYNC_ON, JSF_VSYNC_FAST, JSF_VSYNC_OFF };

typedef struct jsf_config {
    char game_dir[JSF_PATH_MAX];    /* folder holding default.xbe and the rest of the disc */
} jsf_config;

/* Options the overlay menu (F1) changes; saved with the rest. */
typedef struct jsf_settings {
    int  render_scale2;     /* 2-16: render targets at half this multiple of the game's size (3: 1.5x) */
    int  msaa;              /* 1, 2, 4 or 8 samples */
    int  aspect;            /* JSF_ASPECT_*: 16:9 and 21:9 widen the view (src/guest/widescreen.c) */
    bool hud_edges;         /* widescreen: the HUD out to the screen's sides, not centered (d3d_hle.c) */
    int  fit;               /* JSF_FIT_*: how the picture fills the window */
    int  movie_fit;         /* JSF_MOVIE_*: how a movie's picture fills it (renderer.c) */
    bool soft_filter;       /* TV-like vertical softening at presentation */
    int  master_volume;     /* 0-100 % */
    bool show_fps;
    bool gpu_vertex;        /* vertex programs on the GPU (shaders/vsh.vert), not the CPU */
    bool gpu_fixed;         /* fixed-function vertices on the GPU (shaders/fvf.vert) */
    bool frame_interp;      /* frames between the game's 60, blended (renderer.c) */
    int  lsfg_multiplier;   /* Lossless Scaling frame generation: pictures per game
                               frame, 2-4; 1 off (lsfg_present.cpp, from the next start) */
    bool lsfg_performance;  /* its performance mode, lighter and less exact */
    char lsfg_dll[JSF_PATH_MAX];    /* the player's Lossless.dll; "" looks in the usual places */
    bool fullscreen;        /* borderless, the whole monitor (Alt+Enter) */
    bool fxaa;              /* edge smoothing on the finished frame (shaders/screen.frag) */
    int  vsync;             /* JSF_VSYNC_*: how frames reach the screen (renderer.c) */
    bool fps30;             /* Orta's frames shown every other one, a steady 30 fps (renderer.c) */
    bool shadow_original;   /* Shadows: Low, shadow maps at the game's size, not scaled (renderer.c) */
    int  anisotropy;        /* 1 off, 2, 4, 8, 16: anisotropic filtering of smooth-filtered textures */
    int  bloom;             /* 0 off, 1 low, 2 high: on the scene, before the HUD (renderer.c) */
    int  win_x, win_y, win_w, win_h;    /* the window when not fullscreen (w 0: not set yet) */
    bool win_maximized;
    bool mouse_aim;         /* the mouse steers the left stick (input_hle.c) */
    int  mouse_speed;       /* 10-400 % */
    bool unlock_box;        /* every Pandora's Box item open, the save untouched (src/guest/unlocks.c) */
    bool skip_logos;        /* a press during the boot logos goes to the title (src/guest/intro_skip.c) */
    bool skip_launcher;     /* start the game without the launcher (src/main.c; Shift held shows it) */
    int  rumble;            /* controller vibration strength, 0-100 % (src/hle/input_hle.c) */
    int  touch_buttons;     /* Android: JSF_TOUCH_*, when the on-screen controls show (src/android/) */
    int  touch_size;        /* Android: their size, 90-130 % (touch_controls.c) */
    int  touch_opacity;     /* Android: their opacity, 10-100 % */
    char gpu_driver[64];    /* Android: a custom Vulkan driver's folder in gpu_drivers/, "" the
                               system's (src/android/android_gpu.c, from the next start) */
    bool depth_reversed;    /* depth stored as 1 - z (renderer.c, read at start) */
    /* Picture (present.frag), on the finished frame: */
    bool picture_crt;       /* a CRT's darker midtones: x^(2.5/2.2) */
    int  pic_brightness;    /* -50..50, % added */
    int  pic_contrast;      /* 50..150 % */
    int  pic_gamma;         /* 50..200: 100 as is, more brightens the midtones */
    int  pic_saturation;    /* 0..200 % */
    int  audio_out;         /* JSF_AUDIO_*: stereo; headphones (with hrtf off, a model head);
                               or 5.1, 3D sounds around four speakers (mixer.c) */
    bool hrtf;              /* 3D sounds through the Xbox's HRTF filters (on 5.1 shared front and
                               back), else a pan, the model head or the corners (dsound_hle.c) */
} jsf_settings;

/* 21:9 is the game's widescreen mode made wider (2.37:1, 64:27). */
enum { JSF_ASPECT_4_3, JSF_ASPECT_16_9, JSF_ASPECT_21_9, JSF_ASPECT_COUNT };
enum { JSF_AUDIO_STEREO, JSF_AUDIO_HEADPHONES, JSF_AUDIO_SURROUND, JSF_AUDIO_COUNT };
/* Android's on-screen controls: hidden while a gamepad is connected, always shown, or never. */
enum { JSF_TOUCH_AUTO, JSF_TOUCH_ALWAYS, JSF_TOUCH_OFF, JSF_TOUCH_COUNT };
/* The whole picture with bars; stretched to the window; zoomed to fill it,
 * the overflow cut off. */
enum { JSF_FIT_BARS, JSF_FIT_STRETCH, JSF_FIT_ZOOM, JSF_FIT_COUNT };
/* Movies: as the game frames them (Fit to window, the default); or the
 * picture alone, whole (bars where the window's shape differs), stretched
 * or zoomed. */
enum { JSF_MOVIE_ORIGINAL, JSF_MOVIE_BARS, JSF_MOVIE_STRETCH, JSF_MOVIE_ZOOM, JSF_MOVIE_COUNT };

/* Whether the picture is drawn widescreen (anamorphic: a wider view in the
 * game's 640x480), the shape it is meant to be shown at (4/3 when not),
 * and how much narrower a 4:3 x becomes in it (1 at 4:3):
 * src/guest/widescreen.c. */
bool jsf_widescreen_active(void);
float jsf_widescreen_ratio(void);
float jsf_widescreen_k(void);
/* d3d_hle.c, at each Swap: the next frame is a static menu, shown at 4:3. */
void jsf_widescreen_set_menu(bool on);
/* View frustums the game built since the last call (Swap): 0, no 3D view. */
unsigned jsf_widescreen_take_frustums(void);
/* The aspect setting asks for 4:3 in a widescreen game or the other way:
 * that takes a restart (16:9 and 21:9 switch at the next frame). */
bool jsf_widescreen_needs_restart(void);
/* Game thread, once a frame: applies a change between 16:9 and 21:9. */
void jsf_widescreen_frame(void);

/* The current settings: defaults until jsf_config_load reads the file. */
jsf_settings *jsf_settings_get(void);
/* Write the settings, keeping the game folder the file already holds. */
bool jsf_settings_save(void);
/* A setting from the command line (--name value): name as in the settings
 * file (dashes for underscores; a few aliases: scale, output, volume...),
 * value as there. For this run only: a save keeps the file's value unless
 * the setting was changed since. False: no such setting. */
bool jsf_settings_set(const jsf_config *cfg, const char *name, const char *value);
/* Whether jsf_settings_set takes name (before the settings are loaded). */
bool jsf_settings_known(const char *name);
/* The settings into the log, when they differ from the last time. */
void jsf_settings_log(void);

/* Where settings, saves and imported game files live. With a portable.txt
 * next to the executable this is the executable's folder; a new player
 * gets one made there (portable by default); a player with data in the
 * per-user folder keeps it. Always ends in a path separator. */
const char *jsf_data_dir(void);
/* Which of those jsf_data_dir chose and why, for the log. */
extern const char *jsf_data_dir_note;

/* The log file this run writes, empty without one (main.c); a crash report
 * names it. */
extern char jsf_log_file[JSF_PATH_MAX];

/* The console's E: (TDATA, UDATA), where the game keeps its saves (several:
 * it has its own save-file screen): saves/E in the data folder. Written
 * without a trailing separator. */
void jsf_save_dir(char *out, size_t len);

bool jsf_config_load(jsf_config *cfg);
bool jsf_config_save(const jsf_config *cfg);

#endif
