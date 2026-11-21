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
guest memory had been wiped. With DirectSound ported
(`dsound_hle.c`) it plays the LucasArts logo movie (Bink) at 30 fps with
sound streams open. The menus draw; a level was black
(sound playing): its declaration-only vertex shaders spread a vertex over
several streams, now copied together per draw. Then a level load froze
at 90 %: the CRT's tan() looping on a stale x87 C2 (patch 0050). In the
training level the ships draw; A did nothing in the pause menu (a run of
missed wrappers at 0x000683AC, now seeds; the pause menu works since).
The mission stopped after its first voice line: stream packets'
completion events are now set when a packet has played. Input ported. The cloud has default.xbe
only. Facts: `docs/jsf-facts.md`.

## How it fits together

- `tools/recomp.py`: the USA `default.xbe` (or the ISO) -> `gen/` through
  xboxrecomp and patches 0001-0049 (0048: movsx from bp/sp, from Mad
  Dash; 0049: file reads logged only up to 2000, then failures; 0050:
  x87 transcendentals and fprem clear C2, or the CRT's tan() looped;
  0051: xbox_set_guest_event, for HLE code to signal a guest event). Data sections `.data`, `DOLBY` and `BINKDATA` are excluded from
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
- `src/hle/input_hle.c`: Mad Dash's input (from Orta's) at XAPI 4361's
  addresses, with vibration.
- The log's first line names the commit built.

## Next

1. A longer run on the full game, with a controller: the movies, the
   title screen, the menus (Flash, "Strobe", `swf/`). Check the log for
   "declaration-only vertex shader ... not drawn" and "FVF ... not
   handled" lines.
2. The steady kernel calls during the movie (NtWaitForSingleObjectEx and
   NtReleaseMutant about 1.7 million a second each): a thread polling.
3. `d3d_calltrace.c` from `tools/gen_d3d_calltrace.py --symbols
   build/xdk_symbols.txt`.
4. Bink: lines like scanlines through the logo movies (owner's report):
   a capture (F12) of a movie frame, then Bink's blitter or our texture.
5. The app icon (`tools/make_icon.py`) is still Mad Dash's design.

## Testing

- Cloud: gdb (`thread apply all bt`) on a stuck run names the game
  functions; lavapipe (`apt-get install mesa-vulkan-drivers`),
  `SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy`; `JSF_FAST=4`,
  `JSF_SCREENSHOT=dir`, `JSF_CAPTURE=frame`, `JSF_INPUT_SCRIPT` (they need
  the D3D and input hooks). The run does not exit on SIGTERM: use
  `timeout -s KILL`.
- Windows: build from a Developer PowerShell; `--log run.log`, not
  PowerShell redirection. F12 captures a frame, F11 dumps threads.
