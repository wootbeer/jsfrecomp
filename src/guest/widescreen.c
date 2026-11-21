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
 * The game still culls to its own 4:3 view: things can appear late at
 * the far left and right. Its camera (an object whose +0x154 holds 4/3
 * and +0x158 0.75, set up at 0x0005DDD5) is the place to widen that.
 *
 * The setting switches at the next frame.
 */

#include "config.h"

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
