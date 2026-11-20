#ifndef JSF_SETUP_UI_H
#define JSF_SETUP_UI_H

#include "assets.h"
#include "config.h"

#include <SDL3/SDL.h>

#include <stdbool.h>

/* First-run setup: ask the player for their disc image or extracted game
 * folder, check it, import it if needed and save the choice to cfg.
 * Returns false if the player quits. Drawn in window and renderer if
 * given (the launcher's, left open; the player asked to choose, so no
 * search for files first), else in a window of its own. */
bool jsf_setup_run(jsf_config *cfg, jsf_game_check *check, SDL_Window *window, SDL_Renderer *renderer);

#endif
