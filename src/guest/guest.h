#ifndef JSF_GUEST_H
#define JSF_GUEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Map the Xbox address space from the XBE, set up the kernel layer and the
 * function dispatch table. Does not run any game code. */
bool jsf_guest_init(const void *xbe_data, size_t xbe_size,
                    const char *game_dir, const char *save_dir);
/* The game files' folder given to jsf_guest_init. */
const char *jsf_guest_game_dir(void);

/* Run the title from its entry point. Returns when the title exits. */
void jsf_guest_run(uint32_t entry);

/* The guest CPU's registers live per host thread, and jsf_guest_init sets
 * them up on the thread that calls it. To run the title on another thread
 * (made after jsf_guest_init: the Xbox address space has to be mapped at
 * its fixed addresses before a big thread stack can land on them), save
 * them there and load them on the new thread before jsf_guest_run. The
 * guest CPU lock, which the setup gives the calling thread, goes with
 * them. */
typedef struct jsf_guest_cpu {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi, fs_base;
} jsf_guest_cpu;
void jsf_guest_cpu_save(jsf_guest_cpu *c);
void jsf_guest_cpu_load(const jsf_guest_cpu *c);

void jsf_guest_shutdown(void);

/* Run the title from `entry` on this thread with the registers saved by
 * jsf_guest_cpu_save, and again from the start each time it reboots in
 * place (jsf_guest_reboot). Returns when the title exits. */
void jsf_guest_run_boots(const jsf_guest_cpu *cpu, uint32_t entry);

/* Boot the title again in this process, as the console does when a title
 * launches itself (relaunch.c), with `launch_page` as its launch data: memory
 * and the kernel layer as at start-up, our D3D, sound and input state
 * forgotten, the window kept. Called on a guest thread inside the kernel
 * call that rebooted; does not return, unless the kernel layer cannot (no
 * guest CPU lock): false, nothing changed. */
bool jsf_guest_reboot(const uint8_t *launch_page, size_t launch_bytes);

/* Switching between the disc's programs (src/guest/relaunch.c): when a title
 * boots one this build has, JediStarfighterRecomp starts again on it with its launch
 * data. Install before running the title; load the data handed over (from
 * --launch-data) after jsf_guest_init. */
void jsf_relaunch_install(int argc, char **argv);
bool jsf_relaunch_load(const char *file);

/* Write every game file the title opens, once each, to list_path. */
void jsf_guest_log_opened_files(const char *list_path);

/* The game's movie setup opens a movie (src/hle/dsound_hle.c sees it):
 * its .sfd name, a guest address. d3d_hle.c reads the picture's size. */
void jsf_movie_opened(uint32_t sfd_name_va);

#endif
