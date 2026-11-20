# Star Wars Jedi Starfighter: facts

What is known about the game, for the port. Addresses are the USA
default.xbe's (the only one recognised).

## The executable

- Title: Star Wars Jedi Starfighter, title ID 4C410005 (LA-005,
  LucasArts), North America, disc 0, version 1. Retail; entry 0x001C7BB7,
  kernel thunk 0x00282440. Built as `SF2Xbox.exe`.
- Built with XDK 4361 (between JSRF's 4134 and Orta's 4928). Libraries:
  XAPILIB 4361 QFE 2; D3D8, D3DX8, XGRAPHC, DSOUND, XBOXKRNL, LIBCMT 4361
  QFE 1.
- Sections (`tools/xbe_info.py`):

  | Section | Address | Notes |
  |---|---|---|
  | .text | 0x00011000 | game code, 2.19 MB, to 0x002267E0 |
  | D3D | 0x002267E0 | Direct3D 8 |
  | D3DX | 0x00237960 | D3DX8 |
  | XGRPH | 0x00242FE0 | XGraphics |
  | DSOUND | 0x002450A0 | DirectSound |
  | BINK and its 11 blitter sections | 0x00261EC0-0x0027AE00 | Bink video |
  | XPP | 0x0027AE00 | XAPI peripherals (input) |
  | .rdata | 0x00282440 | |
  | .data | 0x002E4F40 | marked executable, is data |
  | DOLBY | 0x00355B00 | the APU's Dolby DSP code, not x86 |
  | BINKDATA | 0x0035CC80 | marked executable, is data |
  | $$XTIMAGE, $$XSIMAGE | 0x00360EC0, 0x003716C0 | title/save images |

- Movies are **Bink** (`lecintro.bik`, `slintro.bik`), as in Mad Dash.
- Engine: LucasArts' own (`CORE\Code\Foundation`, "Europa"), resource
  pak files (`Common.pak` first, "Loading resource pakfile"), a Flash
  player for the menus ("Strobe", Secret Level's Flash Player for Games,
  `Strobe\SWFFont.xpr`). Streams under `Streams\Cine\`,
  `Streams\Cine_5_1\`, `Streams\Dialog\`, `Streams\Music\`,
  `Streams\Video\`.

## Recompiling

- `tools/recomp.py` (xboxrecomp + patches 0001-0047): 21,128 functions,
  0 failed, about 2.24M lines in 85 chunks, 13 unresolved call targets
  stubbed.
- Missed by the sweep, seen as `[ICALL] Failed` at the first boot, now in
  `tools/jsf_seeds.json`: 0x0027B004 and 0x0027B8E9 (XPP, device init
  through a table) and 0x0014D358 (.text, called from 0x001AB990).

## First boot (no XDK hooks, default.xbe only)

- Starts a worker thread (0x001CB326), makes `TDATA\4c410005` and
  `UDATA\4c410005` (TitleMeta.xbx, TitleImage.xbx, SaveImage.xbx), formats
  the cache partition (Partition5).
- Then opens from the disc root, in order: `lec.reg` (missing is fine),
  `commline.txt` (command line, optional), `Common.pak`, `autoexec.con`.
  The game folder check (`src/assets.c`) asks for `Common.pak` or a
  `Streams` folder beside default.xbe.
- Without `Common.pak` it stops there. The disc's full file list is still
  to be confirmed.
