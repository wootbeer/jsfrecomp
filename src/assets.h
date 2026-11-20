#ifndef JSF_ASSETS_H
#define JSF_ASSETS_H

#include "xbe.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct jsf_game_check {
    jsf_xbe_info    xbe;
    jsf_xbe_verdict verdict;
    bool            has_data;
} jsf_game_check;

/* Check that dir holds a usable copy of the game: a recognised default.xbe
 * and other game files. Fills check even on failure where it can. */
bool jsf_assets_check_dir(const char *dir, jsf_game_check *check, char *err, size_t err_len);

typedef void (*jsf_progress_fn)(void *userdata, double fraction, const char *current_file);

/* Extract a disc image into dst_dir (replacing what was there). The XBE is
 * checked before anything else is written, so a wrong disc fails fast. */
bool jsf_assets_import_iso(const char *iso_path, const char *dst_dir,
                           jsf_progress_fn progress, void *userdata,
                           jsf_game_check *check, char *err, size_t err_len);

/* Look for the game without asking: a usable "game" folder, or a disc image,
 * next to the executable or in the data folder. Lets players skip the system
 * file dialog, which may not appear in some environments (e.g. Steam Deck's
 * Gaming Mode). Returns true and fills out/is_iso when something is found. */
bool jsf_assets_autodetect(char *out, size_t len, bool *is_iso);

/* Folder that imported disc images are extracted into. */
void jsf_assets_default_import_dir(char *out, size_t len);

#endif
