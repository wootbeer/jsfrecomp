/*
 * Widescreen for Star Wars Jedi Starfighter: 16:9 and 21:9, ours.
 *
 * The game has no widescreen mode (its only aspect logic is the cutscene
 * letterbox) and renders a 640x480 4:3 frame. With the Aspect setting at
 * 16:9 or 21:9 the picture is drawn anamorphic, as Orta Recomp shows its
 * game's own 16:9 mode: the frame keeps its size, every perspective draw
 * sees a wider field of view, and the window pass shows the frame at the
 * wide shape (renderer.c, present_to_window). In d3d_hle.c: the
 * projection's x scale (fixed function) or the viewport scale in c[58]
 * (vertex programs) times jsf_widescreen_k(); screen-space draws (the HUD,
 * the game's own 2D) squeezed back by the same factor about the middle, or
 * with HUD Edges out toward the sides (hud_squeeze); anything as wide as
 * the screen (backgrounds, fades, the letterbox, movies) stretched.
 *
 * The game culls to its view frustum (0x000169BF), built from its camera:
 * that is widened too (below), or the sun's glow and whatever else it
 * tests went out near the wide screen's sides while still in view. The
 * camera itself (its projection, 0x0001F8BD; world to screen, 0x000271ED)
 * stays 4:3, so the 2D the game places with it lines up with the squeezed
 * HUD.
 *
 * The setting switches at the next frame.
 */

#include "recomp_types.h"
#include "config.h"

#include <SDL3/SDL.h>

#include <math.h>

/* A static menu screen (d3d_hle.c: a menu's backdrop and little 3D)
 * is drawn and shown as the 4:3 the game made it: the Flash menus fill
 * their shapes with bitmaps whose coordinates come from the positions,
 * so a squeezed shape cut its picture off instead of narrowing it. */
static bool menu_43;

void jsf_widescreen_set_menu(bool on)
{
    menu_43 = on;
}

bool jsf_widescreen_active(void)
{
    return jsf_settings_get()->aspect != JSF_ASPECT_4_3 && !menu_43;
}

float jsf_widescreen_ratio(void)
{
    if (menu_43)
        return 4.0f / 3.0f;
    switch (jsf_settings_get()->aspect) {
    case JSF_ASPECT_16_9: return 16.0f / 9.0f;
    case JSF_ASPECT_21_9: return 64.0f / 27.0f;
    default:              return 4.0f / 3.0f;
    }
}

/* How much narrower a 4:3 picture's x becomes in the wide frame: the 3D
 * views' x scale and the 2D draws' squeeze. 1 at 4:3. */
float jsf_widescreen_k(void)
{
    return (4.0f / 3.0f) / jsf_widescreen_ratio();
}

bool jsf_widescreen_needs_restart(void) { return false; }
void jsf_widescreen_frame(void) {}

/* The view frustum from a perspective camera (thiscall, the frustum in
 * ecx; the camera, argument 0; `ret 4`; called from 0x0001A5C8). It takes
 * sin and cos of half the camera's horizontal field of view (+0x90,
 * radians) for the side planes and its height over width (+0x94, 0.75)
 * for the top and bottom. For widescreen the camera is lent a field of
 * view as much wider as the screen is, with the height over width
 * narrowed to match so the vertical view is the same, and given back
 * after: the culling then covers what the wide picture shows. */
/* View frustums built since the last Swap: the game renders a 3D view
 * (none on a loading screen or a menu of its own). */
static unsigned frustums;

unsigned jsf_widescreen_take_frustums(void)
{
    unsigned n = frustums;
    frustums = 0;
    return n;
}

extern void sub_000169BF_gen(void);
void sub_000169BF(void)
{
    uint32_t cam = MEM32(g_esp + 4);
    float k = jsf_widescreen_k(), fov, aspect;

    frustums++;
    if (k >= 1.0f || !cam) {
        sub_000169BF_gen();
        return;
    }
    fov = MEMF(cam + 0x90);
    aspect = MEMF(cam + 0x94);
    if (!(fov > 0.0f && fov < 3.1f)) {
        sub_000169BF_gen();
        return;
    }
    MEMF(cam + 0x90) = 2.0f * atanf(tanf(0.5f * fov) / k);
    MEMF(cam + 0x94) = aspect * k;
    sub_000169BF_gen();
    MEMF(cam + 0x90) = fov;
    MEMF(cam + 0x94) = aspect;
}

/* A target's marker (0x0012204E, thiscall on the marker; the camera in
 * edi): after drawing the bracket it tests whether the target, its screen
 * position (0x000271ED, the 4:3 picture's pixels) plus and minus the
 * bracket's radius, is inside 0..width and 0..height (inline, from
 * 0x00122395), then whether the point is (0x0002726F); if either says no,
 * the bracket's off-screen arrow is drawn, placed from the middle toward
 * that position. Wide, a target out past the 4:3 picture's side is still on
 * the screen, and got an arrow at the 4:3 border as well as its bracket.
 *
 * Just before the tests it calls 0x00121A12 with 0 (thiscall, `ret 4`);
 * here its x there ([ebp+0x38], ebp the caller's: the frame is esp + 0x84
 * at that call) is squeezed toward the middle as the wide picture is, so
 * the tests and the arrow see where the target is on the wide screen in
 * the 4:3 picture's pixels, as the HUD is drawn. The bracket is drawn
 * already; nothing after the tests reads the x. */
extern void sub_00121A12_gen(void);
void sub_00121A12(void)
{
    float k = jsf_widescreen_k();

    if (k < 1.0f && MEM32(g_esp) == 0x00122395u && MEM32(g_esp + 4) == 0u) {
        uint32_t ebp = g_esp + 0x84u, cam = g_edi;
        uint32_t ret = MEM32(ebp + 0x74u);      /* 0x0012204E's own return */
        int w = (int)MEM32(cam + 8);
        float x = MEMF(ebp + 0x38u);
        if (ret >= 0x00010000u && ret < 0x00210000u && w >= 320 && w <= 4096 && isfinite(x) &&
            fabsf(x) < 1.0e6f) {
            static int logged;
            MEMF(ebp + 0x38u) = 0.5f * (float)w + (x - 0.5f * (float)w) * k;
            if (logged++ < 3)
                SDL_Log("widescreen: target marker test, x %.1f -> %.1f (y %.1f, screen %d x %d, from %08x)", x,
                        MEMF(ebp + 0x38u), MEMF(ebp + 0x3cu), w, (int)MEM32(cam + 12), ret);
        }
    }
    sub_00121A12_gen();
}
