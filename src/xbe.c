/*
 * XBE header parsing and game identification.
 *
 * Retail dumps of one release can differ in a few certificate bytes (allowed
 * media), so besides the whole-file hash we also hash the section contents,
 * which is what the recompiled code actually depends on. tools/recomp.py
 * carries the same tables; keep them in step.
 *
 * Star Wars Jedi Starfighter (USA) is LA-005 (LucasArts), built with XDK 4361.
 */

#include "xbe.h"

#include "sha256.h"

#include <stdio.h>
#include <string.h>

#define XBE_RETAIL_ENTRY_KEY  0xA8FC57ABu
#define XBE_DEBUG_ENTRY_KEY   0x94859D4Bu
#define XBE_RETAIL_THUNK_KEY  0x5B6D40B6u
#define XBE_DEBUG_THUNK_KEY   0xEFB1F152u

#define JSF_TITLE_ID 0x4C410005u

/* Known dumps, NULL-terminated. */
static const char *const known_file_sha256[] = {
    "cd9fc79edf4d90b45967a3c6c72bad8ed293647bdf6df91ac832d3c9188d6685",
    NULL,
};

/* Sections whose placement the recompiled code depends on, NULL-terminated. */
static const struct { const char *name; uint32_t vaddr; } jsf_layout[] = {
    { ".text",    0x00011000 },
    { "D3D",      0x002267E0 },
    { "D3DX",     0x00237960 },
    { "XGRPH",    0x00242FE0 },
    { "DSOUND",   0x002450A0 },
    { "BINK",     0x00261EC0 },
    { "XPP",      0x0027AE00 },
    { ".rdata",   0x00282440 },
    { ".data",    0x002E4F40 },
    { "DOLBY",    0x00355B00 },
    { "BINKDATA", 0x0035CC80 },
    { NULL, 0 },
};

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Bounds-checked translation of a header VA into a file offset. */
static bool va_to_off(uint32_t va, uint32_t base, size_t file_size, size_t need, size_t *off)
{
    if (va < base)
        return false;
    *off = (size_t)(va - base);
    return *off <= file_size && need <= file_size - *off;
}

static bool in_any_section(const jsf_xbe_info *x, uint32_t va)
{
    uint32_t i;
    for (i = 0; i < x->num_sections; i++)
        if (va >= x->sections[i].vaddr && va - x->sections[i].vaddr < x->sections[i].vsize)
            return true;
    return false;
}

bool jsf_xbe_parse(const uint8_t *data, size_t size, jsf_xbe_info *out,
                   char *err, size_t err_len)
{
    uint32_t cert_va, sect_va, n, i, raw_entry, raw_thunk, lib_count, lib_va;
    size_t off;
    jsf_sha256 h;

    memset(out, 0, sizeof *out);
    if (size < 0x178 || memcmp(data, "XBEH", 4) != 0) {
        snprintf(err, err_len, "not an Xbox executable (no XBEH header)");
        return false;
    }

    out->base = rd32(data + 0x104);
    cert_va   = rd32(data + 0x118);
    n         = rd32(data + 0x11C);
    sect_va   = rd32(data + 0x120);
    raw_entry = rd32(data + 0x128);
    raw_thunk = rd32(data + 0x158);
    lib_count = rd32(data + 0x160);
    lib_va    = rd32(data + 0x164);

    if (n == 0 || n > JSF_XBE_MAX_SECTIONS) {
        snprintf(err, err_len, "unexpected section count %u", n);
        return false;
    }
    out->num_sections = n;

    if (!va_to_off(cert_va, out->base, size, 0x0C + 80, &off)) {
        snprintf(err, err_len, "certificate outside the file");
        return false;
    }
    out->title_id = rd32(data + off + 0x08);
    for (i = 0; i < 40; i++) {
        uint16_t ch = (uint16_t)(data[off + 0x0C + i * 2] | data[off + 0x0D + i * 2] << 8);
        if (ch == 0)
            break;
        out->title[i] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : '?';
    }

    if (!va_to_off(sect_va, out->base, size, (size_t)n * 0x38, &off)) {
        snprintf(err, err_len, "section table outside the file");
        return false;
    }
    for (i = 0; i < n; i++) {
        const uint8_t *s = data + off + (size_t)i * 0x38;
        jsf_xbe_section *sec = &out->sections[i];
        size_t name_off;

        sec->vaddr    = rd32(s + 0x04);
        sec->vsize    = rd32(s + 0x08);
        sec->raw_addr = rd32(s + 0x0C);
        sec->raw_size = rd32(s + 0x10);
        if (sec->raw_addr > size || sec->raw_size > size - sec->raw_addr) {
            snprintf(err, err_len, "section %u data outside the file", i);
            return false;
        }
        if (va_to_off(rd32(s + 0x14), out->base, size, 1, &name_off)) {
            size_t k;
            for (k = 0; k + 1 < sizeof sec->name && name_off + k < size && data[name_off + k]; k++)
                sec->name[k] = (char)data[name_off + k];
        }
    }

    /* XDK libraries linked in: 8-byte name, then major, minor, build, flags. */
    if (lib_count <= JSF_XBE_MAX_LIBRARIES &&
        va_to_off(lib_va, out->base, size, (size_t)lib_count * 16, &off)) {
        out->num_libraries = lib_count;
        for (i = 0; i < lib_count; i++) {
            const uint8_t *l = data + off + (size_t)i * 16;
            memcpy(out->libraries[i].name, l, 8);
            out->libraries[i].build = (uint16_t)(l[12] | l[13] << 8);
            if (out->libraries[i].build > out->xdk_build)
                out->xdk_build = out->libraries[i].build;
        }
    }

    /* Retail and debug builds scramble these with different keys; the right
     * one is whichever lands inside the image. */
    out->entry = raw_entry ^ XBE_RETAIL_ENTRY_KEY;
    out->kernel_thunk = raw_thunk ^ XBE_RETAIL_THUNK_KEY;
    if (!in_any_section(out, out->entry)) {
        out->entry = raw_entry ^ XBE_DEBUG_ENTRY_KEY;
        out->kernel_thunk = raw_thunk ^ XBE_DEBUG_THUNK_KEY;
    }

    jsf_sha256_init(&h);
    jsf_sha256_update(&h, data, size);
    jsf_sha256_final(&h, out->file_sha256);

    jsf_sha256_init(&h);
    for (i = 0; i < n; i++)
        jsf_sha256_update(&h, data + out->sections[i].raw_addr, out->sections[i].raw_size);
    jsf_sha256_final(&h, out->image_sha256);
    return true;
}

jsf_xbe_verdict jsf_xbe_identify(const jsf_xbe_info *info)
{
    char hex[65];
    size_t i;
    uint32_t j;

    if (info->title_id != JSF_TITLE_ID)
        return JSF_XBE_WRONG_GAME;

    jsf_sha256_hex(info->file_sha256, hex);
    for (i = 0; known_file_sha256[i]; i++)
        if (strcmp(hex, known_file_sha256[i]) == 0)
            return JSF_XBE_KNOWN;

    for (i = 0; jsf_layout[i].name; i++) {
        bool found = false;
        for (j = 0; j < info->num_sections; j++)
            if (strcmp(info->sections[j].name, jsf_layout[i].name) == 0 &&
                info->sections[j].vaddr == jsf_layout[i].vaddr)
                found = true;
        if (!found)
            return JSF_XBE_WRONG_GAME;
    }
    return JSF_XBE_LAYOUT_MATCH;
}

const char *jsf_xbe_verdict_text(jsf_xbe_verdict v)
{
    switch (v) {
    case JSF_XBE_KNOWN:
        return "Star Wars Jedi Starfighter (USA), known dump";
    case JSF_XBE_LAYOUT_MATCH:
        return "Star Wars Jedi Starfighter (USA), layout matches but this exact dump "
               "has not been seen before";
    case JSF_XBE_WRONG_GAME:
        return "a different game or region; only the USA release is supported";
    default:
        return "not an Xbox executable";
    }
}
