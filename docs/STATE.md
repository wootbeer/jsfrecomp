# Current state

v0.0.1, bring-up. The repository was set up from Mad Dash Recomp (itself
from Orta Recomp v0.1.15): launcher, game-file picker (ISO or folder), F1
menu, settings, renderer, mixer, crash reports, Android project and the
xboxrecomp patches, renamed for Jedi Starfighter (`JediStarfighterRecomp`,
`jsfrecomp.ini`, `JSF_*`, `jsf_*`). The game recompiles (21,145 functions,
none failed) and builds. Without hooks, on the owner's full disc, it read
its paks and stopped in the real Direct3D's device set-up, waiting on the
GPU (no window). Orta's Direct3D hooks are now ported (`d3d_hle.c`) but
not yet run: the cloud has default.xbe only. Facts: `docs/jsf-facts.md`.

## How it fits together

- `tools/recomp.py`: the USA `default.xbe` (or the ISO) -> `gen/` through
  xboxrecomp and patches 0001-0048 (0048: movsx from bp/sp, from Mad
  Dash). Data sections `.data`, `DOLBY` and `BINKDATA` are excluded from
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
- `src/hle/hle_pending.c`: stubs of the DirectSound and input reboot
  hooks. Orta's DirectSound and input (`src/hle/orta/`, 4928) and Mad
  Dash's (`src/hle/maddash/`, 3925/3936) wait to be re-addressed.
- The log's first line names the commit built.

## Next

1. Run the D3D hooks on the full game (owner's Windows build): a window,
   CreateDevice's log lines, Swap's frame count, `[ICALL] Failed` lines
   to seed. Then the logos: Bink (`lecintro.bik`, `slintro.bik`), as in
   Mad Dash.
2. DirectSound (DSOUND 4361) and input (XAPI 4361) from Orta's, with
   `build/xdk_symbols.txt`; drop their stubs from `hle_pending.c`.
3. `d3d_calltrace.c` from `tools/gen_d3d_calltrace.py --symbols
   build/xdk_symbols.txt`.
4. Menus are Flash ("Strobe", `swf/`): see how they draw.
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
