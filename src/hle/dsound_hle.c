/*
 * DirectSound, replaced at its API.
 *
 * First written for JSRF (XDK 4134), then Orta Recomp's (XDK 4928; its
 * original is src/hle/orta/dsound_hle.c). Addresses are Jedi Starfighter's
 * (DSOUND 4361), mapped by XbSymbolDatabase name, each `ret N` checked in
 * this game's code: all as Orta's. The hooks are the 34 entry points the
 * game and Bink call (every call into the DSOUND section from outside it),
 * the stream vtable's seven methods, and DirectSoundCreateBuffer, which
 * Orta did not call. Orta's hooks for calls this game never makes, and its
 * 5.1 movie soundtracks (CRI .aix), are not carried over.
 *
 * The title links the XDK's DirectSound library, which drives the Xbox's
 * MCPX audio processor directly; with no such hardware its DirectSoundCreate
 * fails with DSERR_NODRIVER and the title quits. Every DirectSound entry
 * point the game itself calls is replaced here and routed to our mixer
 * (src/audio/mixer.c). The library's internals are then never reached.
 *
 * Conventions: each function is __stdcall. At entry g_esp points at the
 * return address and argument n is at g_esp + 4 + 4n. Each one pops the
 * return address and exactly the argument bytes the original's `ret N` did
 * (measured from the binary), and returns its HRESULT in g_eax.
 *
 * Objects handed to the game are small blocks of guest memory holding a tag
 * and our ids; the game treats them as opaque interface pointers.
 *
 * Not implemented yet, accepted and recorded: envelopes (SetEG), LFOs,
 * filters, I3DL2 source HF levels, obstruction and occlusion, and the DSP
 * image's own effects (its I3DL2 reverb is ours, in
 * src/audio/reverb.c).
 *
 * JSF_DSOUND_TRACE=1 logs every call; JSF_HRTF=n sets the HRTF filters'
 * scale (below).
 */

#include "recomp_types.h"

#include "audio/mixer.h"
#include "config.h"
#include "guest/guest.h"
#include "xbox_memory_layout.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARG(n)      MEM32(g_esp + 4 + 4 * (n))
#define RETURN(argbytes, hr) do { g_eax = (uint32_t)(hr); g_esp += 4 + (argbytes); return; } while (0)

#define DS_OK               0x00000000u
#define DSERR_INVALIDCALL   0x88780032u
#define DSERR_OUTOFMEMORY   0x8007000Eu

#define TAG_DSOUND  0x534F4450u     /* "PDOS" */
#define TAG_BUFFER  0x464F4450u     /* "PDOF" */

/* Object layout in guest memory. */
#define OBJ_TAG     0
#define OBJ_VOICE   4
#define OBJ_REFS    8
#define OBJ_DATA    12   /* buffer memory we allocated, if any */
#define OBJ_BYTES   16
#define OBJ_SIZE    32

static uint32_t the_dsound;

/* 3D state of DSBCAPS_CTRL3D buffers, by voice id (see "3D sound"). */
#define DS3DMODE_HEADRELATIVE 1
#define DS3DMODE_DISABLE      2
#define MAX_3D                513     /* voice ids are 1..512 */

static struct {
    float pos[3], front[3], top[3], rolloff;
} listener = { {0, 0, 0}, {0, 0, 1}, {0, 1, 0}, 1.0f };

static struct {
    bool  used;
    float pos[3], min_dist, max_dist;
    uint32_t mode;
} src3d[MAX_3D];

static int trace_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("JSF_DSOUND_TRACE");
        on = v && *v && *v != '0';
    }
    return on;
}

#define TRACE(...) do { if (trace_on()) SDL_Log("dsound: " __VA_ARGS__); } while (0)

static void note_once(const char *what)
{
    /* Each unimplemented feature is reported the first time it is used. */
    static const char *seen[64];
    for (int i = 0; i < 64; i++) {
        if (seen[i] == what)
            return;
        if (!seen[i]) {
            seen[i] = what;
            SDL_Log("dsound: %s used by the game (accepted, not implemented yet)", what);
            return;
        }
    }
}

static int voice_of(uint32_t obj)
{
    if (!obj || MEM32(obj + OBJ_TAG) != TAG_BUFFER)
        return 0;
    return (int)MEM32(obj + OBJ_VOICE);
}

static uint32_t guest_alloc(uint32_t bytes)
{
    uint32_t va = xbox_HeapAlloc(bytes ? bytes : 4, 32);
    if (va)
        memset((void *)((uintptr_t)va + g_xbox_mem_offset), 0, bytes);
    return va;
}

/* ── IDirectSound ─────────────────────────────────────────────────────── */

/* DirectSoundCreate(pguid, ppDS, pUnkOuter) */
void sub_0024716E(void)
{
    uint32_t pp = ARG(1);

    if (!the_dsound) {
        jsf_mixer_init();
        the_dsound = guest_alloc(OBJ_SIZE);
        if (!the_dsound)
            RETURN(12, DSERR_OUTOFMEMORY);
        MEM32(the_dsound + OBJ_TAG) = TAG_DSOUND;
    }
    MEM32(the_dsound + OBJ_REFS) += 1;
    if (pp)
        MEM32(pp) = the_dsound;
    TRACE("DirectSoundCreate -> 0x%08x", the_dsound);
    RETURN(12, DS_OK);
}


/* IDirectSound_Release(pDS). The device lives for the whole run. */
void sub_002451D3(void)
{
    uint32_t ds = ARG(0), refs = 0;
    if (ds && MEM32(ds + OBJ_REFS))
        refs = (MEM32(ds + OBJ_REFS) -= 1);
    RETURN(4, refs);
}


/* A buffer from a DSBUFFERDESC: dwSize, dwFlags +4, dwBufferBytes +8,
 * lpwfxFormat +12, lpMixBins +16. Returns the HRESULT. */
static uint32_t create_buffer(uint32_t desc, uint32_t pp)
{
    uint32_t flags, bytes, wfx, mixbins, obj, data = 0;
    jsf_voice_desc vd;
    int id;

    if (!desc || !pp)
        return DSERR_INVALIDCALL;
    flags = MEM32(desc + 4);
    bytes = MEM32(desc + 8);
    wfx = MEM32(desc + 12);
    mixbins = MEM32(desc + 16);

    memset(&vd, 0, sizeof vd);
    vd.format = JSF_FMT_PCM16;
    vd.channels = 1;
    vd.sample_rate = 48000;
    vd.positional = (flags & 0x10) != 0;    /* DSBCAPS_CTRL3D */
    if (wfx) {
        uint16_t tag = MEM16(wfx), bits = MEM16(wfx + 14);
        vd.channels = MEM16(wfx + 2) == 2 ? 2 : 1;
        vd.sample_rate = MEM32(wfx + 4);
        if (tag == 0x0069) {
            vd.format = JSF_FMT_XADPCM;
            vd.samples_per_block = MEM16(wfx + 16) >= 2 ? MEM16(wfx + 18) : 64;
        } else {
            vd.format = bits == 8 ? JSF_FMT_PCM8 : JSF_FMT_PCM16;
            if (tag != 1)
                SDL_Log("dsound: unknown wave format 0x%04x, treating as PCM", tag);
        }
    }

    if (mixbins && MEM32(mixbins)) {
        uint32_t n = MEM32(mixbins), pairs = MEM32(mixbins + 4);
        for (uint32_t i = 0; i < n && i < JSF_MAX_VOICE_BINS && pairs; i++) {
            vd.bins[vd.bin_count].bin = MEM32(pairs + i * 8);
            vd.bins[vd.bin_count].volume = (int32_t)MEM32(pairs + i * 8 + 4);
            vd.bin_count++;
        }
    } else {
        /* The default routing: a mono buffer to both front speakers, a stereo
         * one left to left and right to right. */
        vd.bins[0].bin = JSF_BIN_FRONT_LEFT;
        vd.bins[1].bin = JSF_BIN_FRONT_RIGHT;
        vd.bin_count = 2;
    }

    if (bytes) {
        data = guest_alloc(bytes);
        if (!data)
            return DSERR_OUTOFMEMORY;
        vd.data = data;
        vd.bytes = bytes;
    }

    id = jsf_voice_create(&vd);
    obj = guest_alloc(OBJ_SIZE);
    if (!id || !obj)
        return DSERR_OUTOFMEMORY;
    MEM32(obj + OBJ_TAG) = TAG_BUFFER;
    if (id < MAX_3D) {
        memset(&src3d[id], 0, sizeof src3d[id]);
        src3d[id].used = vd.positional;
    }
    MEM32(obj + OBJ_VOICE) = (uint32_t)id;
    MEM32(obj + OBJ_REFS) = 1;
    MEM32(obj + OBJ_DATA) = data;
    MEM32(obj + OBJ_BYTES) = bytes;
    MEM32(pp) = obj;

    TRACE("CreateSoundBuffer voice %d", id);
    TRACE("CreateSoundBuffer flags 0x%x, %u bytes, %s %u ch %u Hz, %d bins -> 0x%08x",
          flags, bytes, vd.format == JSF_FMT_XADPCM ? "adpcm" : vd.format == JSF_FMT_PCM8 ? "pcm8" : "pcm16",
          vd.channels, vd.sample_rate, vd.bin_count, obj);
    for (int i = 0; i < vd.bin_count; i++)
        TRACE("  0x%08x bin %u volume %d", obj, vd.bins[i].bin, vd.bins[i].volume);
    return DS_OK;
}

/* IDirectSound_CreateSoundBuffer(pDS, pdsbd, ppBuffer, pUnkOuter) */
void sub_00246FB4(void)
{
    uint32_t hr = create_buffer(ARG(1), ARG(2));
    RETURN(16, hr);
}

/* DirectSoundCreateBuffer(pdsbd, ppBuffer): the same without a device
 * pointer. 4361's own makes a device with its internal create (0x00247101)
 * and calls CDirectSound::CreateSoundBuffer, neither of them hooked. */
void sub_002471B5(void)
{
    uint32_t hr = create_buffer(ARG(0), ARG(1));
    RETURN(8, hr);
}

/* DirectSoundUseFullHRTF() (called once during sound init): selects the
 * full HRTF tables, which apply_3d reads itself (hrtf_filters). Nothing to
 * set up; takes no arguments. */
void sub_00245204(void)
{
    g_esp += 4;
}

/* DirectSoundDoWork(): the mixer runs on its own thread. */
void sub_00246088(void)
{
    g_esp += 4;
}

/* IDirectSound_CommitDeferredSettings(pDS) */
void sub_002465CB(void) { RETURN(4, DS_OK); }

/* ── DSP effects image ─────────────────────────────────────────────────
 * The title loads a DSP program (reverb and the like) and then reads and
 * writes its parameters by effect index. We return a descriptor whose state
 * segments are plain memory, so parameter traffic works, and record it. */

#define FX_COUNT       32
#define FX_STATE_BYTES 4096
static uint32_t fx_desc;

/* IDirectSound_DownloadEffectsImage(pDS, pvImage, dwSize, pImageLoc, ppDesc) */
void sub_00245EF9(void)
{
    uint32_t pp = ARG(4);

    note_once("DSP effects image (reverb)");
    TRACE("DownloadEffectsImage %u bytes at 0x%08x, image loc 0x%08x", ARG(2), ARG(1), ARG(3));
    if (!fx_desc) {
        fx_desc = guest_alloc(8 + FX_COUNT * 32);
        if (!fx_desc)
            RETURN(20, DSERR_OUTOFMEMORY);
        MEM32(fx_desc) = FX_COUNT;
        for (int i = 0; i < FX_COUNT; i++) {
            uint32_t map = fx_desc + 8 + (uint32_t)i * 32;
            MEM32(map + 8) = guest_alloc(FX_STATE_BYTES);    /* lpvStateSegment */
            MEM32(map + 12) = FX_STATE_BYTES;
        }
    }
    if (pp)
        MEM32(pp) = fx_desc;
    RETURN(20, DS_OK);
}

static uint32_t fx_state(uint32_t index, uint32_t offset, uint32_t size)
{
    uint32_t map;
    if (!fx_desc || index >= FX_COUNT || offset > FX_STATE_BYTES || size > FX_STATE_BYTES - offset)
        return 0;
    map = fx_desc + 8 + index * 32;
    return MEM32(map + 8) + offset;
}



/* ── 3D sound ─────────────────────────────────────────────────────────
 * DSBCAPS_CTRL3D buffers are placed relative to the listener: DirectSound's
 * distance model (full volume up to the minimum distance, then
 * min / (min + rolloff * (d - min)), held from the maximum distance on),
 * then the APU's per-voice HRTF filter from the game's own tables (below;
 * stereo 3D buffers are panned), on 5.1 shared between the front and back
 * speaker pairs as DirectSound does. With 3D sound off: an equal-power pan,
 * on Headphones a model head, on 5.1 a pan around the four corners. No cones or Doppler (Orta sets cones of
 * 360 degrees and no velocity). Deferred settings apply at once. Positions
 * are in the world, the listener moving with the player; enemy gunfire is
 * played at the shooter, within the minimum distance of the player, so at
 * full volume. */

static float fget(uint32_t va)
{
    uint32_t v = MEM32(va);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static float fval(uint32_t v)
{
    float f;
    memcpy(&f, &v, 4);
    return f;
}

/* Sound > 3D sound On (the default): mono 3D voices go through the Xbox's
 * own HRTF filters, as the APU placed them; Off pans them (equal power, on
 * Headphones the model head, on 5.1 around the corners). The
 * APU's scale for the taps is not known: / 512 sounded closest to the
 * console by ear (256 is 6 dB louder); JSF_HRTF=n divides by n instead,
 * JSF_HRTF=0 is the same as Off. DirectSound's lookup
 * (0x002C5497): elevation rounded to 6 degrees, azimuth folded to 0-180 and
 * rounded to 3 degrees (6 above 30 degrees of elevation, 12 above 60, 0 at
 * the poles), a uint16 index of 61 x 31 at 0x002DE590 to a pair of 32-byte
 * entries at 0x002CCFD0 + index * 32. An entry is 31 sign-magnitude taps
 * (the newest sample first) and, in the pair's first entry, the delay in
 * 48 kHz samples. The first entry is the ear away from the sound (the
 * delay, the head's shadow), the second the ear facing it (0x002C7551
 * sends the pair to the APU by the azimuth's sign). */
static float hrtf_scale(void)
{
    static float scale = -1.0f;
    if (scale < 0.0f) {
        const char *e = getenv("JSF_HRTF");
        scale = e ? (float)(atoi(e) > 0 ? atoi(e) : 0) : 512.0f;
        if (e)
            SDL_Log("dsound: JSF_HRTF: %s %g", scale > 0.0f ? "HRTF taps /" : "off,", scale);
    }
    return scale;
}

static int round_to(float v, int step)
{
    return (int)floorf(v / (float)step + 0.5f) * step;
}

/* az: degrees, positive to the right; el: degrees, positive up. On 5.1
 * (surround) DirectSound folds a sound behind onto the front (0x002C5553):
 * the filters place it left or right, the speaker pairs front or back. */
static void hrtf_filters(float az, float el, float scale, bool surround, float h[2][JSF_HRTF_TAPS], float delay[2])
{
    int e = round_to(el, 6), a = 0, row, col;
    uint32_t pair;
    float aa = fabsf(az);

    if (e > 90) e = 90;
    if (e < -90) e = -90;
    if (e != 90 && e != -90)
        a = round_to(aa, e > 60 || e < -60 ? 12 : e > 30 || e < -30 ? 6 : 3);
    if (a > 180) a = 180;
    if (surround && a > 90)
        a = 180 - a;
    row = a / 3;
    col = (e + 90) / 6;
    /* The library's tables (CFullHrtfSource::GetHrtfFilterPair, 0x00247B65):
     * an index per (row, column) at 0x00260418, 32-byte filter pairs at
     * 0x0024EE58. */
    pair = 0x0024EE58u + (uint32_t)MEM16(0x00260418u + 2u * (uint32_t)(row * 31 + col)) * 32u;
    for (int k = 0; k < 2; k++) {
        /* k 0: the far ear (the first entry), 1: the near ear */
        int ear = (az >= 0.0f) == (k == 0) ? 0 : 1;
        uint32_t ent = pair + 32u * (uint32_t)k;
        for (int i = 0; i < JSF_HRTF_TAPS; i++) {
            uint8_t b = MEM8(ent + (uint32_t)i);
            h[ear][i] = (float)(b & 0x80 ? -(int)(b & 0x7F) : (int)b) / scale;
        }
        delay[ear] = k == 0 ? (float)(MEM8(pair + 31u) & 0x7F) : 0.0f;
    }
}

/* On 5.1, DirectSound shares a 3D voice's power between the front and
 * back speaker pairs (0x002C520B; the APU's HRTF filter does left and
 * right): the back share is (1 - |el| / 90) (|az| / 90 - 1) + 0.5, held to
 * 0..1 -- all front within 45 degrees of ahead, all back within 45 of
 * behind, even overhead. A voice nearer than half a unit (its distance
 * times the listener's distance factor and a per-buffer scale, taken as 1:
 * Orta's are 1 and the scale is internal) is drawn towards even, by half
 * its distance. Each share becomes a volume, 1000 log10 (share). */
static float back_share(float az, float el, float d)
{
    float v = (1.0f - fabsf(el) / 90.0f) * (fabsf(az) / 90.0f - 1.0f) + 0.5f;
    v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    if (d < 0.5f)
        v = 0.5f + (v - 0.5f) * d * 0.5f;
    return v;
}

static void apply_3d(int id)
{
    float rel[3], right[3], d, g, x = 0.0f, l, r;
    const float *f = listener.front, *t = listener.top;

    if (id <= 0 || id >= MAX_3D || !src3d[id].used)
        return;
    if (src3d[id].mode == DS3DMODE_DISABLE) {
        jsf_voice_set_3d_distance(id, 1.0f);
        jsf_voice_set_3d(id, 1.0f, 1.0f);
        return;
    }
    for (int k = 0; k < 3; k++)
        rel[k] = src3d[id].pos[k] - (src3d[id].mode == DS3DMODE_HEADRELATIVE ? 0.0f : listener.pos[k]);
    d = sqrtf(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
    {
        float mn = src3d[id].min_dist > 0 ? src3d[id].min_dist : 1.0f;
        float mx = src3d[id].max_dist > mn ? src3d[id].max_dist : 1e9f;
        float dc = d < mn ? mn : d > mx ? mx : d;
        g = mn / (mn + listener.rolloff * (dc - mn));
    }
    jsf_voice_set_3d_distance(id, g);
    /* Right = top x front (D3D is left-handed); head-relative uses the
     * listener's own axes, which are the identity there. */
    if (src3d[id].mode == DS3DMODE_HEADRELATIVE) {
        right[0] = 1; right[1] = 0; right[2] = 0;
    } else {
        right[0] = t[1] * f[2] - t[2] * f[1];
        right[1] = t[2] * f[0] - t[0] * f[2];
        right[2] = t[0] * f[1] - t[1] * f[0];
    }
    if (d > 1e-6f) {
        float n = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
        if (n > 1e-6f)
            x = (rel[0] * right[0] + rel[1] * right[1] + rel[2] * right[2]) / (d * n);
    }
    if (jsf_settings_get()->hrtf && hrtf_scale() > 0.0f) {
        const float fr[3] = { 0.0f, 0.0f, 1.0f }, up[3] = { 0.0f, 1.0f, 0.0f };
        const bool hr = src3d[id].mode == DS3DMODE_HEADRELATIVE;
        const float *ff = hr ? fr : f, *tt = hr ? up : t;
        float fz = 0.0f, y = 0.0f, h[2][JSF_HRTF_TAPS], delay[2];
        float nf = sqrtf(ff[0] * ff[0] + ff[1] * ff[1] + ff[2] * ff[2]);
        float nt = sqrtf(tt[0] * tt[0] + tt[1] * tt[1] + tt[2] * tt[2]);
        if (d > 1e-6f && nf > 1e-6f)
            fz = (rel[0] * ff[0] + rel[1] * ff[1] + rel[2] * ff[2]) / (d * nf);
        if (d > 1e-6f && nt > 1e-6f)
            y = (rel[0] * tt[0] + rel[1] * tt[1] + rel[2] * tt[2]) / (d * nt);
        if (y > 1.0f) y = 1.0f;
        if (y < -1.0f) y = -1.0f;
        {
            float az = atan2f(x, fz) * 57.2957795f, el = asinf(y) * 57.2957795f, back = 0.0f;
            bool surround = jsf_settings_get()->audio_out == JSF_AUDIO_SURROUND;
            if (surround)
                back = back_share(az, el, d);
            hrtf_filters(az, el, hrtf_scale(), surround, h, delay);
            jsf_voice_set_3d_hrtf(id, g, h, delay, sqrtf(1.0f - back), sqrtf(back));
        }
        return;
    }
    /* Headphones (a model head) and 5.1 (around four speakers), the output
     * setting: both need ahead or behind too. */
    if (jsf_settings_get()->audio_out != JSF_AUDIO_STEREO) {
        float fz = 0.0f;
        const float fr[3] = { 0.0f, 0.0f, 1.0f };
        const float *ff = src3d[id].mode == DS3DMODE_HEADRELATIVE ? fr : f;
        float n = sqrtf(ff[0] * ff[0] + ff[1] * ff[1] + ff[2] * ff[2]);
        if (d > 1e-6f && n > 1e-6f)
            fz = (rel[0] * ff[0] + rel[1] * ff[1] + rel[2] * ff[2]) / (d * n);
        if (jsf_settings_get()->audio_out == JSF_AUDIO_SURROUND)
            jsf_voice_set_3d_surround(id, g, x, fz);
        else
            jsf_voice_set_3d_ears(id, g, x, fz);
        return;
    }
    /* Equal-power pan: a source straight ahead is -3 dB in each ear. */
    l = sqrtf(0.5f * (1.0f - x));
    r = sqrtf(0.5f * (1.0f + x));
    jsf_voice_set_3d(id, g * l, g * r);
}

static void apply_3d_all(void)
{
    for (int id = 1; id < MAX_3D; id++)
        if (src3d[id].used)
            apply_3d(id);
}


/* IDirectSound_SetOrientation(pDS, xFront, yFront, zFront, xTop, yTop, zTop, apply) */
void sub_00246C1F(void)
{
    for (int k = 0; k < 3; k++) {
        listener.front[k] = fval(ARG(1 + k));
        listener.top[k] = fval(ARG(4 + k));
    }
    apply_3d_all();
    RETURN(32, DS_OK);
}

/* IDirectSound_SetPosition(pDS, x, y, z, apply) */
void sub_00246C69(void)
{
    for (int k = 0; k < 3; k++)
        listener.pos[k] = fval(ARG(1 + k));
    TRACE("listener SetPosition %g %g %g", listener.pos[0], listener.pos[1], listener.pos[2]);
    apply_3d_all();
    RETURN(20, DS_OK);
}


/* ── IDirectSoundBuffer ───────────────────────────────────────────────── */



/* IDirectSoundBuffer_Release(pBuf) */
void sub_002451EE(void)
{
    uint32_t b = ARG(0), refs = 0;
    int id = voice_of(b);

    if (id && MEM32(b + OBJ_REFS)) {
        refs = (MEM32(b + OBJ_REFS) -= 1);
        if (!refs) {
            if (id < MAX_3D)
                src3d[id].used = false;
            jsf_voice_destroy(id);
            MEM32(b + OBJ_TAG) = 0;
            if (MEM32(b + OBJ_DATA))
                xbox_HeapFree(MEM32(b + OBJ_DATA));
            xbox_HeapFree(b);
        }
    }
    TRACE("Release 0x%08x (voice %d) -> %u", b, id, refs);
    RETURN(4, refs);
}

/* IDirectSoundBuffer_SetBufferData(pBuf, pvData, dwBytes): play the game's
 * own memory. */
void sub_00246C9E(void)
{
    int id = voice_of(ARG(0));
    if (!id)
        RETURN(12, DSERR_INVALIDCALL);
    jsf_voice_set_data(id, ARG(1), ARG(2));
    TRACE("SetBufferData 0x%08x: 0x%08x, %u bytes", ARG(0), ARG(1), ARG(2));
    RETURN(12, DS_OK);
}

/* IDirectSoundBuffer_Lock(pBuf, offset, bytes, ppv1, pdw1, ppv2, pdw2, flags) */
void sub_00246044(void)
{
    uint32_t b = ARG(0), off = ARG(1), n = ARG(2), flags = ARG(7);
    uint32_t data = MEM32(b + OBJ_DATA), size = MEM32(b + OBJ_BYTES);
    uint32_t n1, n2;
    int id = voice_of(b);

    if (!id || !data || !size)
        RETURN(32, DSERR_INVALIDCALL);
    if (flags & 2)              /* DSBLOCK_ENTIREBUFFER */
        off = 0, n = size;
    else if (flags & 1)         /* DSBLOCK_FROMWRITECURSOR */
        off = jsf_voice_position(id);
    off %= size;
    if (n > size)
        n = size;
    n1 = size - off < n ? size - off : n;
    n2 = n - n1;
    TRACE("Lock 0x%08x offset %u, %u bytes, flags 0x%x", b, off, n, flags);
    if (ARG(3)) MEM32(ARG(3)) = data + off;
    if (ARG(4)) MEM32(ARG(4)) = n1;
    if (ARG(5)) MEM32(ARG(5)) = n2 ? data : 0;
    if (ARG(6)) MEM32(ARG(6)) = n2;
    RETURN(32, DS_OK);
}

/* IDirectSoundBuffer_Unlock(pBuf, pv1, dw1, pv2, dw2): the voice reads the
 * memory itself. */
void sub_002451E9(void)
{
    RETURN(20, DS_OK);
}

/* IDirectSoundBuffer_Play(pBuf, reserved1, reserved2, flags) */
void sub_00245F90(void)
{
    int id = voice_of(ARG(0));
    if (!id)
        RETURN(16, DSERR_INVALIDCALL);
    if (ARG(3) & 2)             /* DSBPLAY_FROMSTART */
        jsf_voice_set_position(id, 0);
    jsf_voice_play(id, (ARG(3) & 1) != 0);
    TRACE("Play 0x%08x flags 0x%x", ARG(0), ARG(3));
    RETURN(16, DS_OK);
}

/* IDirectSoundBuffer_Stop(pBuf) */
void sub_00245FB4(void)
{
    int id = voice_of(ARG(0));
    if (id)
        jsf_voice_stop(id);
    TRACE("Stop 0x%08x", ARG(0));
    RETURN(4, DS_OK);
}

/* IDirectSoundBuffer_GetStatus(pBuf, pdwStatus) */
void sub_00245FEC(void)
{
    int id = voice_of(ARG(0));
    bool looping = false, playing = id && jsf_voice_playing(id, &looping);
    if (ARG(1))
        MEM32(ARG(1)) = (playing ? 1u : 0u) | (playing && looping ? 4u : 0u);
    TRACE("GetStatus 0x%08x -> %s", ARG(0), playing ? "playing" : "stopped");
    RETURN(8, id ? DS_OK : DSERR_INVALIDCALL);
}

/* IDirectSoundBuffer_GetCurrentPosition(pBuf, pdwPlay, pdwWrite) */
void sub_00246008(void)
{
    int id = voice_of(ARG(0));
    uint32_t pos = id ? jsf_voice_position(id) : 0;
    TRACE("GetCurrentPosition 0x%08x -> %u", ARG(0), pos);
    if (ARG(1)) MEM32(ARG(1)) = pos;
    if (ARG(2)) MEM32(ARG(2)) = pos;
    RETURN(12, id ? DS_OK : DSERR_INVALIDCALL);
}

/* IDirectSoundBuffer_SetCurrentPosition(pBuf, dwOffset) */
void sub_00246028(void)
{
    int id = voice_of(ARG(0));
    if (id)
        jsf_voice_set_position(id, ARG(1));
    RETURN(8, DS_OK);
}

/* IDirectSoundBuffer_SetLoopRegion(pBuf, dwStart, dwLength) */
void sub_00245FCC(void)
{
    int id = voice_of(ARG(0));
    if (id)
        jsf_voice_set_loop_region(id, ARG(1), ARG(2));
    TRACE("SetLoopRegion 0x%08x %u, %u bytes", ARG(0), ARG(1), ARG(2));
    RETURN(12, DS_OK);
}

/* IDirectSoundBuffer_SetVolume(pBuf, lVolume) */
void sub_00245F20(void)
{
    int id = voice_of(ARG(0));
    if (id)
        jsf_voice_set_volume(id, (int32_t)ARG(1));
    TRACE("SetVolume 0x%08x %d", ARG(0), (int32_t)ARG(1));
    RETURN(8, DS_OK);
}

/* IDirectSoundBuffer_SetHeadroom(pBuf, dwHeadroom) */
void sub_00245F3C(void)
{
    int id = voice_of(ARG(0));
    if (id)
        jsf_voice_set_headroom(id, ARG(1));
    TRACE("SetHeadroom 0x%08x %u", ARG(0), ARG(1));
    RETURN(8, DS_OK);
}

/* IDirectSoundBuffer_SetFrequency(pBuf, dwFrequency) */
void sub_002465E3(void)
{
    int id = voice_of(ARG(0));
    TRACE("SetFrequency 0x%08x %u Hz", ARG(0), ARG(1));
    if (id)
        jsf_voice_set_frequency(id, ARG(1));
    RETURN(8, DS_OK);
}

/* IDirectSoundBuffer_SetMixBins(pBuf, pMixBins) */
void sub_00245F58(void)
{
    int id = voice_of(ARG(0));
    uint32_t mb = ARG(1);
    jsf_voice_bin bins[JSF_MAX_VOICE_BINS];
    int n = 0;

    if (id && mb) {
        uint32_t count = MEM32(mb), pairs = MEM32(mb + 4);
        for (uint32_t i = 0; i < count && n < JSF_MAX_VOICE_BINS && pairs; i++, n++) {
            bins[n].bin = MEM32(pairs + i * 8);
            bins[n].volume = (int32_t)MEM32(pairs + i * 8 + 4);
        }
        jsf_voice_set_bins(id, bins, n);
    }
    for (int i = 0; i < n; i++)
        TRACE("  0x%08x bin %u volume %d", ARG(0), bins[i].bin, bins[i].volume);
    RETURN(8, DS_OK);
}


/* Per-buffer 3D state (see "3D sound" above). */
static int src_of(uint32_t buf)
{
    int id = voice_of(buf);
    return id > 0 && id < MAX_3D && src3d[id].used ? id : 0;
}


/* IDirectSoundBuffer_SetMaxDistance(pBuf, f, apply) */
void sub_002465FF(void)
{
    int id = src_of(ARG(0));
    if (id) { src3d[id].max_dist = fval(ARG(1)); apply_3d(id); }
    RETURN(12, DS_OK);
}

/* IDirectSoundBuffer_SetMinDistance(pBuf, f, apply) */
void sub_00246623(void)
{
    int id = src_of(ARG(0));
    if (id) { src3d[id].min_dist = fval(ARG(1)); apply_3d(id); }
    RETURN(12, DS_OK);
}


/* IDirectSoundBuffer_SetPosition(pBuf, x, y, z, apply) */
void sub_00246647(void)
{
    int id = src_of(ARG(0));
    if (id) {
        for (int k = 0; k < 3; k++)
            src3d[id].pos[k] = fval(ARG(1 + k));
        TRACE("3D 0x%08x SetPosition %g %g %g apply %u", ARG(0), src3d[id].pos[0], src3d[id].pos[1],
              src3d[id].pos[2], ARG(4));
        apply_3d(id);
    }
    RETURN(20, DS_OK);
}
/* IDirectSoundBuffer_SetDopplerFactor(pBuf, f, apply): no Doppler. */
void sub_0024667C(void) { RETURN(12, DS_OK); }

/* ── More of IDirectSoundBuffer ──────────────────────────────────────── */


/* IDirectSoundBuffer_SetMixBinVolumes(pBuf, pMixBins): new volumes for the
 * listed bins. Our voices hold only the bins they route to, so this sets
 * the same list (the game passes the whole set). */
void sub_00245F74(void)
{
    int id = voice_of(ARG(0));
    uint32_t mb = ARG(1);
    jsf_voice_bin bins[JSF_MAX_VOICE_BINS];
    int n = 0;

    if (id && mb) {
        uint32_t count = MEM32(mb), pairs = MEM32(mb + 4);
        for (uint32_t i = 0; i < count && n < JSF_MAX_VOICE_BINS && pairs; i++, n++) {
            bins[n].bin = MEM32(pairs + i * 8);
            bins[n].volume = (int32_t)MEM32(pairs + i * 8 + 4);
        }
        jsf_voice_set_bins(id, bins, n);
    }
    for (int i = 0; i < n; i++)
        TRACE("  0x%08x bin %u volume %d", ARG(0), bins[i].bin, bins[i].volume);
    RETURN(8, DS_OK);
}

/* IDirectSoundBuffer_Pause(pBuf, dwPause): DSBPAUSE_RESUME 0, DSBPAUSE_PAUSE
 * 1, DSBPAUSE_SYNCHPAUSE 2. Pausing keeps the position; resuming restarts
 * only a paused buffer. Restarting any stopped one brought back looping
 * gunshots the game had stopped, every time its sound code resumed its
 * buffers, until they piled up. */
static void pause_voice(int id, uint32_t how)
{
    if (id)
        jsf_voice_pause(id, how != 0);
}


/* ── IDirectSoundStream ───────────────────────────────────────────────────
 * A stream is an XMediaObject: the game hands it packets with Process and
 * polls their status. Our stream objects carry the library's own vtable
 * (CDirectSoundStream, 0x0032DD9C), whose methods are replaced below, so
 * calls through the vtable and through the C wrappers both land here.
 *
 * Packets play back to back on a mixer voice (jsf_voice_queue). The
 * descriptor the game passes may be on its stack, so each packet's
 * pdwCompletedSize, pdwStatus and completion event are copied when it is
 * queued, and written from the mixer thread when it has played. */

#define TAG_STREAM          0x53524450u     /* "PDRS" */
#define STREAM_VTABLE       0x002BB644u     /* AddRef, Release, GetInfo, GetStatus, Process, Discontinuity, Flush */

#define STR_VTBL    0
#define STR_TAG     4
#define STR_VOICE   8
#define STR_REFS    12
#define STR_INDEX   16
#define STR_SIZE    32

#define XMEDIAPACKET_STATUS_SUCCESS 0x00000000u
#define XMEDIAPACKET_STATUS_PENDING 0x8000000Au
#define XMEDIAPACKET_STATUS_FLUSHED 0x80004004u
#define XMO_STATUSF_ACCEPT_INPUT_DATA 0x00000001u

#define MAX_STREAMS     32
#define STREAM_PACKETS  64

typedef struct stream_packet {
    uint32_t completed_size, status, event, bytes;
} stream_packet;

typedef struct stream {
    uint32_t      obj;
    int           voice;
    uint32_t      max_packets;
    uint32_t      callback, context;
    uint32_t      block_align;
    uint32_t      next;                 /* next packet slot */
    stream_packet packets[STREAM_PACKETS];
} stream;

static stream streams[MAX_STREAMS];

static stream *stream_of(uint32_t obj)
{
    uint32_t i;

    if (!obj || MEM32(obj + STR_TAG) != TAG_STREAM)
        return NULL;
    i = MEM32(obj + STR_INDEX);
    return i < MAX_STREAMS && streams[i].obj == obj ? &streams[i] : NULL;
}

/* Mixer thread: a packet has been played, or flushed. */
static void packet_done(void *user, uint32_t tag, bool flushed)
{
    stream *s = user;
    stream_packet *p = &s->packets[tag % STREAM_PACKETS];

    if (p->completed_size)
        MEM32(p->completed_size) = flushed ? 0 : p->bytes;
    if (p->status)
        MEM32(p->status) = flushed ? XMEDIAPACKET_STATUS_FLUSHED : XMEDIAPACKET_STATUS_SUCCESS;
}

/* DirectSoundCreateStream(pdssd, ppStream). DSSTREAMDESC: dwFlags,
 * dwMaxAttachedPackets, lpwfxFormat, lpfnCallback, lpvContext, lpMixBins. */
void sub_0024720C(void)
{
    uint32_t desc = ARG(0), pp = ARG(1), wfx, mixbins, obj;
    jsf_voice_desc vd;
    stream *s = NULL;
    int i;

    if (!desc || !pp)
        RETURN(8, DSERR_INVALIDCALL);
    for (i = 0; i < MAX_STREAMS && !s; i++)
        if (!streams[i].obj)
            s = &streams[i];
    if (!s)
        RETURN(8, DSERR_OUTOFMEMORY);
    if (!the_dsound)
        jsf_mixer_init();

    wfx = MEM32(desc + 8);
    mixbins = MEM32(desc + 20);
    memset(&vd, 0, sizeof vd);
    vd.format = JSF_FMT_PCM16;
    vd.channels = 2;
    vd.sample_rate = 48000;
    if (wfx) {
        uint16_t tag = MEM16(wfx), bits = MEM16(wfx + 14);
        vd.channels = MEM16(wfx + 2) == 1 ? 1 : 2;
        vd.sample_rate = MEM32(wfx + 4);
        if (tag == 0x0069) {
            vd.format = JSF_FMT_XADPCM;
            vd.samples_per_block = MEM16(wfx + 16) >= 2 ? MEM16(wfx + 18) : 64;
        } else {
            vd.format = bits == 8 ? JSF_FMT_PCM8 : JSF_FMT_PCM16;
            if (tag != 1)
                SDL_Log("dsound: stream format 0x%04x, treating as PCM", tag);
        }
    }
    if (mixbins && MEM32(mixbins)) {
        uint32_t n = MEM32(mixbins), pairs = MEM32(mixbins + 4);
        for (uint32_t k = 0; k < n && k < JSF_MAX_VOICE_BINS && pairs; k++) {
            vd.bins[vd.bin_count].bin = MEM32(pairs + k * 8);
            vd.bins[vd.bin_count].volume = (int32_t)MEM32(pairs + k * 8 + 4);
            vd.bin_count++;
        }
    } else {
        vd.bins[0].bin = JSF_BIN_FRONT_LEFT;
        vd.bins[1].bin = JSF_BIN_FRONT_RIGHT;
        vd.bin_count = 2;
    }

    memset(s, 0, sizeof *s);
    s->voice = jsf_voice_create(&vd);
    obj = guest_alloc(STR_SIZE);
    if (!s->voice || !obj)
        RETURN(8, DSERR_OUTOFMEMORY);
    s->obj = obj;
    s->max_packets = MEM32(desc + 4) ? MEM32(desc + 4) : 1;
    if (s->max_packets > STREAM_PACKETS)
        s->max_packets = STREAM_PACKETS;
    s->callback = MEM32(desc + 12);
    s->context = MEM32(desc + 16);
    s->block_align = jsf_voice_block_align(s->voice);
    MEM32(obj + STR_VTBL) = STREAM_VTABLE;
    MEM32(obj + STR_TAG) = TAG_STREAM;
    MEM32(obj + STR_VOICE) = (uint32_t)s->voice;
    MEM32(obj + STR_REFS) = 1;
    MEM32(obj + STR_INDEX) = (uint32_t)(s - streams);
    jsf_voice_set_packet_done(s->voice, packet_done, s);
    jsf_voice_play(s->voice, false);    /* plays packets as they arrive */
    if (s->callback)
        note_once("stream packet callbacks");
    MEM32(pp) = obj;
    SDL_Log("dsound: stream 0x%08x: %s %u ch %u Hz, %u packets", obj,
            vd.format == JSF_FMT_XADPCM ? "adpcm" : vd.format == JSF_FMT_PCM8 ? "pcm8" : "pcm16",
            vd.channels, vd.sample_rate, s->max_packets);
    RETURN(8, DS_OK);
}

/* CDirectSoundStream::AddRef(this) */
void sub_00245A08(void)
{
    uint32_t o = ARG(0);
    RETURN(4, stream_of(o) ? (MEM32(o + STR_REFS) += 1) : 0);
}

/* CDirectSoundStream::Release(this) */
void sub_00245A4F(void)
{
    uint32_t o = ARG(0), refs = 0;
    stream *s = stream_of(o);

    if (s && MEM32(o + STR_REFS)) {
        refs = (MEM32(o + STR_REFS) -= 1);
        if (!refs) {
            jsf_voice_flush(s->voice);
            jsf_voice_destroy(s->voice);
            MEM32(o + STR_TAG) = 0;
            s->obj = 0;
            xbox_HeapFree(o);
        }
    }
    RETURN(4, refs);
}

/* CDirectSoundStream::GetInfo(this, pInfo): XMEDIAINFO { dwFlags,
 * dwInputSize, dwOutputSize, dwMaxLookahead }. */
void sub_00245A9D(void)
{
    stream *s = stream_of(ARG(0));
    uint32_t info = ARG(1);

    if (!s || !info)
        RETURN(8, DSERR_INVALIDCALL);
    MEM32(info + 0) = 0x00000005u;      /* XMO_STREAMF_FIXED_SAMPLE_SIZE | INPUT_ASYNC */
    MEM32(info + 4) = s->block_align;
    MEM32(info + 8) = 0;
    MEM32(info + 12) = 0;
    RETURN(8, DS_OK);
}

/* CDirectSoundStream::GetStatus(this, pdwStatus) */
void sub_00245B9E(void)
{
    stream *s = stream_of(ARG(0));

    if (!s)
        RETURN(8, DSERR_INVALIDCALL);
    if (ARG(1))
        MEM32(ARG(1)) = (uint32_t)jsf_voice_queued(s->voice) < s->max_packets
                      ? XMO_STATUSF_ACCEPT_INPUT_DATA : 0;
    RETURN(8, DS_OK);
}

/* CDirectSoundStream::Process(this, pInputPacket, pOutputPacket).
 * XMEDIAPACKET: pvBuffer, dwMaxSize, pdwCompletedSize, pdwStatus,
 * hCompletionEvent (pContext with a callback), prtTimestamp. */
void sub_00245BEF(void)
{
    stream *s = stream_of(ARG(0));
    uint32_t in = ARG(1), slot;
    stream_packet *p;

    if (!s || !in)
        RETURN(12, DSERR_INVALIDCALL);
    if ((uint32_t)jsf_voice_queued(s->voice) >= s->max_packets)
        RETURN(12, DSERR_INVALIDCALL);
    slot = s->next++ % STREAM_PACKETS;
    p = &s->packets[slot];
    p->bytes = MEM32(in + 4);
    p->completed_size = MEM32(in + 8);
    p->status = MEM32(in + 12);
    p->event = MEM32(in + 16);
    if (p->event && !s->callback)
        note_once("stream packet completion events");
    if (p->completed_size)
        MEM32(p->completed_size) = 0;
    if (p->status)
        MEM32(p->status) = XMEDIAPACKET_STATUS_PENDING;
    if (!jsf_voice_queue(s->voice, MEM32(in), p->bytes, slot)) {
        if (p->status)
            MEM32(p->status) = XMEDIAPACKET_STATUS_FLUSHED;
        RETURN(12, DSERR_INVALIDCALL);
    }
    TRACE("stream 0x%08x Process 0x%08x, %u bytes", ARG(0), MEM32(in), p->bytes);
    RETURN(12, DS_OK);
}

/* CDirectSoundStream::Discontinuity(this): the end of the data for now;
 * what is queued still plays. */
void sub_00245B04(void)
{
    RETURN(4, stream_of(ARG(0)) ? DS_OK : DSERR_INVALIDCALL);
}

/* CDirectSoundStream::Flush(this) */
void sub_00245B51(void)
{
    stream *s = stream_of(ARG(0));
    if (s)
        jsf_voice_flush(s->voice);
    RETURN(4, s ? DS_OK : DSERR_INVALIDCALL);
}

static int stream_voice(uint32_t obj)
{
    stream *s = stream_of(obj);
    return s ? s->voice : 0;
}

/* IDirectSoundStream_* setters (5-byte thunks into CDirectSoundStream, or
 * small wrappers), same arguments as the buffer ones. */
/* SetVolume(pStream, lVolume) */
void sub_00246074(void)
{
    int id = stream_voice(ARG(0));
    if (id)
        jsf_voice_set_volume(id, (int32_t)ARG(1));
    TRACE("stream SetVolume 0x%08x %d", ARG(0), (int32_t)ARG(1));
    RETURN(8, DS_OK);
}

/* SetHeadroom(pStream, dwHeadroom) */
void sub_00246079(void)
{
    int id = stream_voice(ARG(0));
    if (id)
        jsf_voice_set_headroom(id, ARG(1));
    TRACE("stream SetHeadroom 0x%08x %u", ARG(0), ARG(1));
    RETURN(8, DS_OK);
}


/* SetMixBins(pStream, pMixBins) */
void sub_0024607E(void)
{
    int id = stream_voice(ARG(0));
    uint32_t mb = ARG(1);
    jsf_voice_bin bins[JSF_MAX_VOICE_BINS];
    int n = 0;

    if (id && mb) {
        uint32_t count = MEM32(mb), pairs = MEM32(mb + 4);
        for (uint32_t i = 0; i < count && n < JSF_MAX_VOICE_BINS && pairs; i++, n++) {
            bins[n].bin = MEM32(pairs + i * 8);
            bins[n].volume = (int32_t)MEM32(pairs + i * 8 + 4);
        }
        jsf_voice_set_bins(id, bins, n);
    }
    for (int i = 0; i < n; i++)
        TRACE("  0x%08x bin %u volume %d", ARG(0), bins[i].bin, bins[i].volume);
    RETURN(8, DS_OK);
}

/* Pause(pStream, dwPause) */
void sub_00246083(void)
{
    pause_voice(stream_voice(ARG(0)), ARG(1));
    RETURN(8, DS_OK);
}



/* ── Warm reboot (src/guest/guest.c) ────────────────────────────────────────
 * The title boots again in place: its voices and objects go, and the
 * listener is back where it starts. */
void jsf_dsound_reboot(void)
{
    jsf_mixer_drop_voices();
    the_dsound = 0;
    memset(src3d, 0, sizeof src3d);
    memset(streams, 0, sizeof streams);
    listener.pos[0] = listener.pos[1] = listener.pos[2] = 0.0f;
    listener.front[0] = 0.0f; listener.front[1] = 0.0f; listener.front[2] = 1.0f;
    listener.top[0] = 0.0f; listener.top[1] = 1.0f; listener.top[2] = 0.0f;
    listener.rolloff = 1.0f;
}
