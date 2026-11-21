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

## Second run (D3D hooks, owner's Windows build)

- A window: CreateDevice 640x480, back buffer format 0x7, flags 0x2A,
  presentation interval 1; back buffers at 0x00236CEC and 0x00236D04.
  113 frames at about 17 fps, 540 draws, all dropped (one vertex shader
  0x010E66E1 set without CreateVertexShader).
- The game reads `common.pak` a byte at a time (473,000 reads; patch
  0049 stops logging each one).
- Then `sound\dsstdfx.bin` (DirectSound's effects image, 24,824 bytes),
  three opens of the disc root, and a crash in XAPI's SetLastError
  (0x001C66E3): the main thread's TIB at guest 0x4000 had fs:[4] = 0, so
  the TLS slot (index -5) read 0xFFFFFFEC. A null-pointer write wiped
  low guest memory; the real DirectSound (no hooks yet then) driving an
  absent audio processor is the likely writer. `jsf_guest_tib_watch`
  logs the frame it happens.

## DirectSound (DSOUND 4361)

- Called from outside the DSOUND section: 34 entry points (game and
  Bink), plus the audio processor's destructor 0x00248BAF through a
  thunk (never reached with our objects). Streams are also called through
  their vtable at 0x002BB644 (AddRef, Release, GetInfo, GetStatus,
  Process, Discontinuity, Flush).
- Every `ret N` as Orta's 4928. Mix bins are DSMIXBINS lists (as 4928).
- DirectSoundCreateBuffer (0x002471B5) uses the library's internal
  create (0x00247101) and CDirectSound::CreateSoundBuffer, so it has a
  hook of its own.
- HRTF tables: index 0x00260418 (row * 31 + column), 32-byte filter
  pairs 0x0024EE58.

## Third run (DirectSound hooks)

- No crash: the 30 s test script closed it. DirectSound's effects image
  accepted, three streams (pcm16 stereo 48 kHz, adpcm mono 22,050 Hz,
  pcm16 stereo), then `autoexec.con`, `pc.con`, `ps2.con`, `cine.con`,
  `util.con`, `fm.con`, `user.con` (the engine's console scripts) and
  `Streams\Video\english\lecintro.bik`, played at 30 fps, one draw a
  frame.
- Before the movie every draw was dropped: vertex shader 0x010E66E1 was
  made by CreateVertexShader(declaration, NULL): declaration-only. JSF's
  CreateVertexShader keeps no raw program: the object is +0 refcount,
  +4 flags (0x10 has a program, 1 not type 0x2078, 8 type 0x7378), +8
  instructions, +0xC size in dwords, +0x1C 16 attribute slots, +0x114 the
  push-buffer commands that load the program (chunks of 32 under method
  0x0B00).
- During the movie, kernel ordinals 234 and 221 run about 1.7 million
  times a second each.
- Input: the game passes 0x0027AED4 (gamepad type) to XInputOpen and
  XGetDeviceChanges, 0x0027AE38 (memory units) to XGetDevices.

## Fourth run (into a level)

- Logos, title and menus draw (menus about 257,000 draws per 5 s, all
  drawn; 35 pixel shaders by then). The owner saw lines like scanlines
  through the logo movies: to check (Bink's blitter, or the movie as
  made).
- In the level almost every draw was dropped (about 60,000 per 5 s):
  the game's declaration-only shaders put each attribute in a stream of
  its own (STREAM(0) position, STREAM(1) diffuse or normal, STREAM(2)
  texture coordinates 0, STREAM(3) coordinates 1...). Now each draw
  copies them together into one FVF vertex (`gather_decl`).
- A shadow map: a 2048x1024 R5G6B5 target at 0x00408000, depth only,
  compared with SHADOWFUNC 0x204.
- Four more functions reached only indirectly in a level (0x0001FBD4,
  0x000E3A4B, 0x0010F1D0, 0x0016823E): seeds.

## Fifth run (level load)

- Every draw drawn (declaration shaders as FVF 0x2..0x252, streams copied
  together). The training level's load stopped at about 90 %, after the
  dialog streams (`streams\dialog\english\...\*_adi.wav`) and a failed
  open of `t01.pak` (not on the disc: optional?).
- The main thread spun in the CRT's tan() (0x00203C5D): fptan, then
  `fnstsw ax; sahf; jp` and fprem1 until C2 clears; neither lifted
  instruction set C2, so a stale unordered compare kept the loop going.
  Patch 0050: 28 sites in this XBE.

## Sixth run (training level)

- The ships draw: vertex programs (78 instructions, run on the GPU through
  the interpreter, a pixel shader each), DXT textures, the shadow map at
  stage 2. The level's intro shot shows the player's ship fly off.
- The owner could not play on: A did nothing in the pause menu. The log
  has an `[ICALL] Failed` for 0x00068449, a `mov ecx, [esp+4]; jmp`
  wrapper in a run of small wrappers (0x000683AC-0x0006858F) the sweep
  missed: 18 of them, now seeds. Likely the menus' (Flash) callbacks.
- `texture coordinate generation 0x30000 not handled` (camera-space
  reflection vector): something shiny lacks its reflection.
