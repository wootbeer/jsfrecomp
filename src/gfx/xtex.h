#ifndef JSF_XTEX_H
#define JSF_XTEX_H

/*
 * Xbox texture formats to RGBA8.
 *
 * An Xbox D3D pixel container describes itself in two words:
 *   Format: bits 8-15 D3DFORMAT, 16-19 mip levels, 20-23 log2 width,
 *           24-27 log2 height, 28-31 log2 depth (swizzled formats)
 *   Size:   linear formats only: bits 0-11 width-1, 12-23 height-1,
 *           24-31 pitch/64-1
 * Swizzled formats store texels in Morton order (u and v bits interleaved,
 * u first); DXT formats are 4x4 blocks in rows; LIN_ formats are rows.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct jsf_xtex_info {
    uint32_t format;        /* D3DFORMAT */
    uint32_t width, height; /* level 0 */
    uint32_t pitch;         /* bytes per row (linear) or 0 */
    uint32_t levels;
    uint32_t bytes;         /* level 0 size in bytes */
    bool     known;         /* a format we can decode */
} jsf_xtex_info;

/* Fill info from a D3DPixelContainer's Format and Size words. */
void jsf_xtex_describe(uint32_t format_word, uint32_t size_word, jsf_xtex_info *info);
/* Whether the format word's color format is a linear one (it then needs
 * a Size word: the NV2A has it as an image rect and a pitch instead). */
bool jsf_xtex_is_linear(uint32_t format_word);

/* Mip levels stored after level 0 (swizzled and DXT textures; a linear
 * texture has one): how many levels there are, and level n's info and byte
 * offset from the texture's start. */
uint32_t jsf_xtex_level_count(const jsf_xtex_info *info);
void jsf_xtex_level(const jsf_xtex_info *info, uint32_t level, jsf_xtex_info *out, uint32_t *offset);

/* Decode one level (info from jsf_xtex_describe or jsf_xtex_level) into tightly packed RGBA8 (width*height*4 bytes). Unknown
 * formats become magenta so they are easy to spot on screen. */
void jsf_xtex_decode(const jsf_xtex_info *info, const uint8_t *src, uint8_t *rgba);
/* 1, 2 or 3 for DXT1, DXT3 and DXT5, which the GPU's BC1, BC2 and BC3 read
 * as they are (blocks in order, not swizzled); else 0. */
int jsf_xtex_bc(const jsf_xtex_info *info);
/* A level's bytes as stored: blocks for DXT. */
uint32_t jsf_xtex_bc_bytes(const jsf_xtex_info *info);

const char *jsf_xtex_format_name(uint32_t format);

#endif
