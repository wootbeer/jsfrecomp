#ifndef JSF_PROGRAM_H
#define JSF_PROGRAM_H

#include <stdbool.h>

/* The programs this build can run. Star Wars Jedi Starfighter has one, default.xbe;
 * the table is kept (from Orta Recomp, which had two) so a second program
 * would slot in the same way. Its generated code links in under
 * jsf_recomp_lookup... (src/guest/jsf_names.h). */
typedef enum jsf_program {
    JSF_PROGRAM_MAIN,   /* default.xbe: Star Wars Jedi Starfighter */
    JSF_PROGRAM_COUNT
} jsf_program;

/* "jsf": --program and the logs. */
const char *jsf_program_name(jsf_program p);
/* Parse a name; false if it is none of them. */
bool jsf_program_from_name(const char *name, jsf_program *out);
/* Its XBE in the game files: "default.xbe". */
const char *jsf_program_xbe(jsf_program p);
/* Image hash of the XBE it was generated from, or NULL when this build does
 * not have it. */
const char *jsf_program_built_from(jsf_program p);

/* The program the guest runs: recomp_lookup and recomp_lookup_manual, which
 * the kernel layer calls (thread starts, APCs, callbacks), go to its tables.
 * Set before jsf_guest_init. */
void jsf_program_select(jsf_program p);
jsf_program jsf_program_active(void);

#endif
