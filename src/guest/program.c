/*
 * Which recompiled program runs. Star Wars Jedi Starfighter has one (default.xbe).
 *
 * The generated code and our sources are compiled with
 * src/guest/jsf_names.h force-included, so the dispatch is
 * jsf_recomp_lookup and friends. The kernel layer and the host's shared
 * code call the unprefixed names; they are defined here.
 */

#include "guest/program.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef void (*recomp_func_t)(void);

extern recomp_func_t jsf_recomp_lookup(uint32_t xbox_va);
extern recomp_func_t jsf_recomp_lookup_manual(uint32_t xbox_va);
extern int jsf_recomp_dispatch_init(void);
extern size_t jsf_recomp_dispatch_flat_bytes(void);
extern const char *jsf_built_from(void);

static jsf_program active = JSF_PROGRAM_MAIN;

static const char *const names[JSF_PROGRAM_COUNT] = { "jsf" };
static const char *const xbes[JSF_PROGRAM_COUNT] = { "default.xbe" };

const char *jsf_program_name(jsf_program p) { return names[p]; }
const char *jsf_program_xbe(jsf_program p) { return xbes[p]; }

bool jsf_program_from_name(const char *name, jsf_program *out)
{
    int i;

    for (i = 0; i < JSF_PROGRAM_COUNT; i++) {
        if (strcmp(name, names[i]) == 0) {
            *out = (jsf_program)i;
            return true;
        }
    }
    return false;
}

const char *jsf_program_built_from(jsf_program p)
{
    return p == JSF_PROGRAM_MAIN ? jsf_built_from() : NULL;
}

void jsf_program_select(jsf_program p) { active = p; }
jsf_program jsf_program_active(void) { return active; }

recomp_func_t recomp_lookup(uint32_t xbox_va) { return jsf_recomp_lookup(xbox_va); }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { return jsf_recomp_lookup_manual(xbox_va); }
int recomp_dispatch_init(void) { return jsf_recomp_dispatch_init(); }
size_t recomp_dispatch_flat_bytes(void) { return jsf_recomp_dispatch_flat_bytes(); }

/* F11 and hangs: the guest threads' waits. Orta's src/hle/wait_trace.c
 * (now src/hle/orta/) hooked XAPI's waits at Orta's addresses; until it is
 * re-addressed for Jedi Starfighter there is nothing to list. */
void jsf_dump_guest_waits(void) {}
