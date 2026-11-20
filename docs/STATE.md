# Current state

v0.0.1, bring-up. The repository was set up from Mad Dash Recomp (itself
from Orta Recomp v0.1.15): launcher, game-file picker (ISO or folder), F1
menu, settings, renderer, mixer, crash reports, Android project and the 47
xboxrecomp patches, renamed for Jedi Starfighter (`JediStarfighterRecomp`,
`jsfrecomp.ini`, `JSF_*`, `jsf_*`). The game recompiles (21,128 functions,
none failed), builds, and boots with no XDK hooks as far as its first disc
reads: it opens `lec.reg`, `commline.txt`, `Common.pak` and `autoexec.con`
from the disc root and stops without `Common.pak`. Facts so far:
`docs/jsf-facts.md`.

## How it fits together

- `tools/recomp.py`: the USA `default.xbe` (or the ISO) -> `gen/` through
  xboxrecomp and patches 0001-0047. Data sections `.data`, `DOLBY` and
  `BINKDATA` are excluded from the code sweep. 3 seeds from the first boot
  (`tools/jsf_seeds.json`).
- `src/xbe.c`, `tools/recomp.py`: title ID 4C410005, the one known dump
  hash and the section layout (keep the two in step).
- `src/assets.c`: a game folder needs `Common.pak` or `Streams` beside
  default.xbe (to confirm against a full disc listing).
- `src/guest/`: bring-up (`guest.c`), the dispatch (`program.c`, one
  program; `jsf_names.h`), ICALL/UNIMPL diagnostics (`guest_manual.c`),
  crash reports, vertex programs on the CPU (`vsh.c`), placeholder
  `widescreen.c`.
- `src/hle/hle_pending.c`: stubs of the reboot hooks only. No Direct3D,
  DirectSound or input hooks are built yet: Orta's (`src/hle/orta/`, XDK
  4928) and Mad Dash's (`src/hle/maddash/`, XDK 3925/3936) wait to be
  re-addressed (`docs/hle-port.md`).

## Next

1. XDK symbols: XbSymbolDatabaseCLI on the XBE into
   `build/xdk_symbols.txt` (the owner's, or built here from a local clone).
2. Re-address the D3D hooks from Orta's (4928, the closer XDK): the frame
   clock, CreateDevice, Clear, Swap/Present, KickOff, BlockOnTime first,
   then the draw path; Mad Dash's push-buffer reader if the game writes
   NV2A methods itself.
3. DirectSound and input (XAPI 4361) from Orta's.
4. The full game files: the disc's file list (folder check), then boot
   with `Common.pak` and the rest to the LucasArts logo (`lecintro.bik`,
   Bink as in Mad Dash).
5. Menus are Flash ("Strobe"): check how they draw once D3D works.

## Testing

- Cloud: gdb (`thread apply all bt`) on a stuck run names the game
  functions; lavapipe (`apt-get install mesa-vulkan-drivers`),
  `SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy`; `JSF_FAST=4`,
  `JSF_SCREENSHOT=dir`, `JSF_CAPTURE=frame`, `JSF_INPUT_SCRIPT` (they need
  the D3D and input hooks). The run does not exit on SIGTERM: use
  `timeout -s KILL`.
- Windows: build from a Developer PowerShell; `--log run.log`, not
  PowerShell redirection. F12 captures a frame, F11 dumps threads.
