#ifndef JSF_INPUT_MAP_H
#define JSF_INPUT_MAP_H

/*
 * Which key and which controller button play each Xbox button, changed in
 * the options overlay (Controls) and saved with the settings. The sticks
 * stay where they are: the left and right sticks, and WASD/IJKL on the
 * keyboard.
 */

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>

enum {
    JSF_IN_A, JSF_IN_B, JSF_IN_X, JSF_IN_Y, JSF_IN_BLACK, JSF_IN_WHITE, JSF_IN_LTRIG, JSF_IN_RTRIG,
    JSF_IN_START, JSF_IN_BACK, JSF_IN_UP, JSF_IN_DOWN, JSF_IN_LEFT, JSF_IN_RIGHT, JSF_IN_LTHUMB, JSF_IN_RTHUMB,
    JSF_IN_COUNT
};

/* A controller input: a button (SDL_GamepadButton), a trigger
 * (JSF_PAD_AXIS + SDL_GamepadAxis), or none (-1). */
#define JSF_PAD_AXIS 100

typedef struct jsf_binding {
    SDL_Scancode key;       /* SDL_SCANCODE_UNKNOWN: none */
    int          pad;
    int          mouse;     /* SDL_BUTTON_LEFT..X2, 0: none */
} jsf_binding;

jsf_binding *jsf_input_bindings(void);          /* JSF_IN_COUNT of them */
const char *jsf_input_button_name(int b);       /* "A", "Start", ... */
void jsf_input_reset(void);                     /* the defaults */
/* How far a binding's controller input is pressed, 0..255 (triggers are
 * analog, buttons 0 or 255); 0 without a controller. */
int jsf_input_pad_level(SDL_Gamepad *g, int pad);
const char *jsf_input_pad_name(int pad);        /* "a", "lefttrigger", "-" */
const char *jsf_input_mouse_name(int mouse);    /* "left", ... "-" */
const char *jsf_input_pad_label(int pad);       /* short, for the menu: "A", "RB", "LT" */

/* Mouse motion while the game has the mouse (renderer.c adds it, in window
 * pixels; input_hle.c takes it once a game frame). */
void jsf_mouse_add(float dx, float dy);
void jsf_mouse_take(float *dx, float *dy);
/* Whether the game has the mouse (held in the window): mouse buttons play
 * the game only then, so the click that takes the mouse back does not. */
void jsf_mouse_set_captured(bool on);
SDL_MouseButtonFlags jsf_mouse_buttons(void);    /* none while not captured */

/* Settings file lines (config.c): key_<button>=<key name>,
 * pad_<button>=<controller input name>. */
bool jsf_input_load_line(const char *name, const char *value);
size_t jsf_input_save(char *out, size_t cap);

#endif
