/*
 * A check on the C runtime's x87 math as recompiled.
 *
 * Its one-argument functions go through a dispatcher (_trandisp1,
 * 0x00208629): edx points at the function's table (a length-prefixed name,
 * then a handler per class of argument: normal, zero, NaN, infinity...),
 * the argument is the double at esp+4, the result comes back in st(0).
 * In this XBE only exp uses it (its table at 0x0033A662, from 0x002041FE).
 * The menus came out zoomed in once exp's handlers were reached (they had
 * been missing, and the calls skipped), so each result is compared with
 * the host's exp(): the first mismatches are logged with their argument,
 * and a count every 1000 calls while any differ.
 */

#include "recomp_types.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

extern void sub_00208629_gen(void);
void sub_00208629(void)
{
    static uint32_t calls, bad;
    uint32_t table = g_edx, lo = MEM32(g_esp + 4), hi = MEM32(g_esp + 8);
    uint64_t bits = (uint64_t)hi << 32 | lo;
    double arg, got, want;

    memcpy(&arg, &bits, sizeof arg);
    sub_00208629_gen();
    if (table != 0x0033A662u)          /* exp's table; no other function here */
        return;
    got = g_fp_stack[g_fp_top & 7];
    want = exp(arg);
    calls++;
    if (!(fabs(got - want) <= 1e-9 * fabs(want) + 1e-300) && !(isnan(got) && isnan(want))) {
        if (bad++ < 20)
            SDL_Log("crt: exp(%.17g) = %.17g recompiled, %.17g expected (call %u, fp top %d)", arg, got, want,
                    calls, g_fp_top & 7);
    }
    if (calls % 1000 == 0 && bad)
        SDL_Log("crt: exp: %u of %u calls wrong so far", bad, calls);
    if (calls == 1)
        SDL_Log("crt: exp is called (first: exp(%.17g) = %.17g)", arg, got);
}

/* JSF_CRT_SELFTEST=1: before the title starts, call its exp (0x002041F8,
 * cdecl, a double) on a few arguments and log each result beside the
 * host's, through the same check. */
extern void sub_002041F8(void);
void jsf_crt_selftest(void)
{
    static const double args[] = { 0.0, 1.0, -1.0, 0.5, 2.302585092994046, -3.25, 10.0, 1e-5, -20.0, 88.0 };

    if (!SDL_getenv("JSF_CRT_SELFTEST"))
        return;
    for (unsigned i = 0; i < SDL_arraysize(args); i++) {
        uint64_t bits;
        double got;
        int top = g_fp_top;
        memcpy(&bits, &args[i], sizeof bits);
        g_esp -= 8;
        MEM32(g_esp) = (uint32_t)bits;
        MEM32(g_esp + 4) = (uint32_t)(bits >> 32);
        g_esp -= 4;
        MEM32(g_esp) = 0;               /* return address: the callee pops it */
        sub_002041F8();
        g_esp += 8;
        got = g_fp_stack[g_fp_top & 7];
        SDL_Log("crt selftest: exp(%.17g) = %.17g, host %.17g%s (fp top %d -> %d)", args[i], got, exp(args[i]),
                fabs(got - exp(args[i])) <= 1e-9 * exp(args[i]) ? "" : "  WRONG", top & 7, g_fp_top & 7);
        g_fp_top = top;
    }
}

/* A thunk reached through a table (jmp 0x000351E9; `[ICALL] Failed` in a
 * level, seeded with exp's handlers). JSF_SKIP_184657=1 skips it as the
 * calls were before it was seeded (return 0, nothing done): to test
 * whether the menus' changed looks come from it. */
extern void sub_00184657_gen(void);
void sub_00184657(void)
{
    static int skip = -1;

    if (skip < 0) {
        skip = SDL_getenv("JSF_SKIP_184657") && *SDL_getenv("JSF_SKIP_184657") == '1';
        if (skip)
            SDL_Log("crt: JSF_SKIP_184657: the thunk 0x00184657 is skipped, as before it was seeded");
    }
    if (skip) {
        g_esp += 4;             /* as an unresolved call: the return address popped */
        g_eax = 0;
        return;
    }
    sub_00184657_gen();
}
