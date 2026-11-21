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

#include <math.h>

bool jsf_widescreen_active(void)
{
    return jsf_settings_get()->aspect != JSF_ASPECT_4_3;
}

float jsf_widescreen_ratio(void)
{
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
extern void sub_000169BF_gen(void);
void sub_000169BF(void)
{
    uint32_t cam = MEM32(g_esp + 4);
    float k = jsf_widescreen_k(), fov, aspect;

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
