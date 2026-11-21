# Current state

v0.0.1, bring-up. The repository was set up from Mad Dash Recomp (itself
from Orta Recomp v0.1.15): launcher, game-file picker (ISO or folder), F1
menu, settings, renderer, mixer, crash reports, Android project and the
xboxrecomp patches, renamed for Jedi Starfighter (`JediStarfighterRecomp`,
`jsfrecomp.ini`, `JSF_*`, `jsf_*`). The game recompiles (21,145 functions,
none failed) and builds. Without hooks, on the owner's full disc, it read
its paks and stopped in the real Direct3D's device set-up, waiting on the
GPU (no window). With Orta's Direct3D hooks (`d3d_hle.c`) it opened a
window and ran 113 frames (every draw dropped), loaded DirectSound's
effects image and crashed in SetLastError: the main thread's TIB in low
guest memory had been wiped. DirectSound is now ported too
(`dsound_hle.c`), the likely fix; a TIB watch logs it if not. The cloud
has default.xbe only. Facts: `docs/jsf-facts.md`.

## How it fits together

- `tools/recomp.py`: the USA `default.xbe` (or the ISO) -> `gen/` through
  xboxrecomp and patches 0001-0049 (0048: movsx from bp/sp, from Mad
  Dash; 0049: file reads logged only up to 2000, then failures). Data sections `.data`, `DOLBY` and `BINKDATA` are excluded from
  the code sweep. 20 seeds from boots (`tools/jsf_seeds.json`).
- `src/xbe.c`, `tools/recomp.py`: title ID 4C410005, the one known dump
  hash and the section layout (keep the two in step).
- `src/assets.c`: a game folder needs `Common.pak` or `Streams` beside
  default.xbe (both are on the disc, in lower case).
- `src/guest/`: bring-up (`guest.c`), the dispatch (`program.c`, one
  program; `jsf_names.h`), ICALL/UNIMPL diagnostics (`guest_manual.c`),
  crash reports, vertex programs on the CPU (`vsh.c`), placeholder
  `widescreen.c`.
- `src/hle/d3d_hle.c`: Orta's Direct3D at JSF's addresses (XDK 4361,
  render states numbered as 4134; `docs/hle-port.md`).
- `src/hle/dsound_hle.c`: Orta's DirectSound at JSF's addresses.
- `src/hle/hle_pending.c`: the input reboot hook's stub. Orta's input
  (`src/hle/orta/`, 4928) and Mad Dash's (`src/hle/maddash/`) wait to be
  re-addressed.
- The log's first line names the commit built.

## Next

1. Run with DirectSound on the full game (owner's Windows build): past
   the crash? If the TIB watch fires, find the writer.
2. Why every draw is dropped (the first: vertex shader 0x010E66E1 never
   created through CreateVertexShader; others FVF or no stream?).
   `JSF_D3D_TRACE=1` lists the first frames' calls.
3. Input (XAPI 4361) from Orta's; drop its stub from `hle_pending.c`.
4. `d3d_calltrace.c` from `tools/gen_d3d_calltrace.py --symbols
   build/xdk_symbols.txt`.
5. Menus are Flash ("Strobe", `swf/`): see how they draw.
6. The app icon (`tools/make_icon.py`) is still Mad Dash's design.

## Testing

- Cloud: gdb (`thread apply all bt`) on a stuck run names the game
  functions; lavapipe (`apt-get install mesa-vulkan-drivers`),
  `SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy`; `JSF_FAST=4`,
  `JSF_SCREENSHOT=dir`, `JSF_CAPTURE=frame`, `JSF_INPUT_SCRIPT` (they need
  the D3D and input hooks). The run does not exit on SIGTERM: use
  `timeout -s KILL`.
- Windows: build from a Developer PowerShell; `--log run.log`, not
  PowerShell redirection. F12 captures a frame, F11 dumps threads.
