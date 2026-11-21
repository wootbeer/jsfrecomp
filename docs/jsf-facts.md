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
- Without `Common.pak` it stops there.

## The disc

- Root: `default.xbe`, `LEC.reg`, `autoexec.con`, `common.pak`,
  `english.pak`, `resource.pak` (about 150 MB), `training.pak` and one pak
  a mission, `00_coruscant.pak` to `16_fleet.pak` (all in lower case).
  Folders: `streams` (almost 8,000 files: cutscenes, dialog, music,
  video), `swf` (the Flash menus), `saveimages`, `strobe`, `sound`.
- With the whole disc (owner's Windows run, no XDK hooks) it opens
  `lec.reg`, `commline.txt` (absent), `resource.pak`, `english.pak`, then
  creates the Direct3D device: the real D3D's register writes (`out dx, al`
  at 0x0022F9C9, `wbinvd` at 0x0022D952), 17 render-state setters reached
  only through D3D's tables (0x00227BD0-0x00228D30, now seeds) and a wait
  on the GPU: no window, a core spinning.

## Direct3D (D3D8 4361)

- XbSymbolDatabase finds 326 symbols (`build/xdk_symbols.txt`, never
  committed). D3D's API is the ordinary one (SetStreamSource, Draw*,
  SetTransform, Swap, SetMaterial...): the game does not write NV2A methods
  itself as Mad Dash does, as far as the library's names go.
- The device is a static struct at 0x00234C60; `D3D_g_pDevice`
  (0x00237740) points at it. +0 push-buffer cursor, +4 its end, +8 flags
  (0x10 pure device, 0x2000 KickOff's rare path), +0x35C the cursor when
  flag 4, +0x207C the back buffers (GetBackBuffer), +0x2264 the GPU channel
  registers (DMA PUT at +0x40), +0x2430 vblank callback, +0x0398 enabled
  lights (linked at +0x8C), +0xB98 the vertex constant shadow (slot = D3D
  register + 96).
- `D3D_g_RenderState` 0x00234718, texture states 0x00234518 ([4][32]).
  Numbering as 4134: simple 0-81 (encode table 0x00282E80, from state 0),
  deferred from 82 (FogEnable), complex from 117 (PSTextureModes). Orta's
  4928 numbers: deferred + 10, complex + 19. Point states 106-113.
- 4361 has one `SetVertexShaderConstant(Register, pData, Count)` (ret 0xC)
  and the 4134-style `GetBackBuffer(index, type, ppSurface)` (ret 0xC).
  Not found: CopyRects, DrawIndexedVerticesUP, SetGammaRamp,
  Begin/SetVertexData/End.
- KickOff (0x0022C760) masks DMA PUT to 27 bits; sync mode flag
  0x00234508. SetShaderConstantMode loads slots 60-62 from 0x002334F0.

## Lifter fix 0048 (movsx from bp/sp)

Four sites here sign-extend now: Bink's 0x00271160 and 0x002721C0, game
code 0x001FEB80 and 0x001FEC60.
