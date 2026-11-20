#ifndef JSF_XBE_H
#define JSF_XBE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JSF_XBE_MAX_SECTIONS  64
#define JSF_XBE_MAX_LIBRARIES 64

typedef struct jsf_xbe_section {
    char     name[16];
    uint32_t vaddr, vsize;
    uint32_t raw_addr, raw_size;
} jsf_xbe_section;

typedef struct jsf_xbe_library {
    char     name[9];
    uint16_t build;             /* XDK build, e.g. 4134 */
} jsf_xbe_library;

typedef struct jsf_xbe_info {
    uint32_t base;
    uint32_t entry;             /* decoded (un-XORed) entry point VA */
    uint32_t kernel_thunk;      /* decoded kernel thunk table VA */
    uint32_t title_id;
    char     title[41];         /* ASCII rendering of the certificate title */
    uint32_t num_sections;
    jsf_xbe_section sections[JSF_XBE_MAX_SECTIONS];
    uint32_t num_libraries;
    jsf_xbe_library libraries[JSF_XBE_MAX_LIBRARIES];
    uint16_t xdk_build;         /* highest library build: the XDK version */
    uint8_t  file_sha256[32];   /* whole file */
    uint8_t  image_sha256[32];  /* section contents only; ignores the certificate */
} jsf_xbe_info;

typedef enum jsf_xbe_verdict {
    JSF_XBE_KNOWN,          /* a dump we have seen before */
    JSF_XBE_LAYOUT_MATCH,   /* Jedi Starfighter (NTSC-U) section layout, not a known dump */
    JSF_XBE_WRONG_GAME,     /* a valid XBE, but not the game we recompile */
    JSF_XBE_INVALID,        /* not an XBE */
} jsf_xbe_verdict;

/* Parse an in-memory XBE. On failure writes a reason to err and returns false. */
bool jsf_xbe_parse(const uint8_t *data, size_t size, jsf_xbe_info *out,
                   char *err, size_t err_len);

jsf_xbe_verdict jsf_xbe_identify(const jsf_xbe_info *info);

/* Human-readable explanation of a verdict, for dialogs and logs. */
const char *jsf_xbe_verdict_text(jsf_xbe_verdict v);

#endif
