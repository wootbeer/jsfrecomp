# Jedi Starfighter Recomp: Star Wars Jedi Starfighter (2002 Xbox) Recompiled and Port

You will need the game files from a licensed copy in order to play it.

A native PC (and later Android) port of Star Wars Jedi Starfighter, made by statically
recompiling the Xbox game with [xboxrecomp](https://github.com/sp00nznet/xboxrecomp),
with graphics, audio and input of its own on [SDL 3](https://libsdl.org).
Built the same way as, and from the code of,
[Orta Recomp](https://github.com/wootbeer/ortarecomp) and Mad Dash Recomp.

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own copy of the game to play this.

**Work in progress, not released yet.** No builds to download: build it
yourself (below) if you want to follow along. Tested on Windows.

## Status

- Boots to the logos (Bink movies), the title and the menus, with sound
  and music.
- The first training mission plays through: flight, the HUD, voice lines,
  the pause menu.
- Controllers (with vibration) and keyboard and mouse.
- Widescreen 16:9 and 21:9: a wider view, the HUD centered or out at the
  screen's edges (in progress: menus show at 4:3).
- Higher resolutions, MSAA/FXAA, frame interpolation and the rest of the
  F1 menu, from Orta Recomp.

Known issues:

- Some menus come out zoomed in at higher render scales (being looked
  into).
- Shiny reflections (environment mapping) are not drawn yet.
- Only the first missions have been tried; later ones may hit missing
  pieces.

## Features (from Orta Recomp's launcher and F1 menu)

- Game file picker: your disc image (ISO) or an extracted game folder,
  checked against the known USA release
- Higher resolutions, widescreen, MSAA/FXAA, anisotropic filtering
- Frame interpolation above 60 fps, Lossless Scaling frame generation
- Color correction, CRT look, bloom
- 3D sound, 5.1, reverb, master volume
- Controllers with vibration, keyboard and mouse, rebindable; touch controls
  on Android (untested)
- Save slots, shader cache

## Game files

You need your own copy of **Star Wars Jedi Starfighter (USA)**: the disc
image (`.iso` / `.xiso`), or the extracted game folder with its
`default.xbe`, `Common.pak` and the rest of the disc.

default.xbe SHA-256:
cd9fc79edf4d90b45967a3c6c72bad8ed293647bdf6df91ac832d3c9188d6685

## Building

See [BUILDING.md](BUILDING.md).

## Credits

- [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) by sp00nz and
  contributors: the recompiler and the Xbox kernel layer this port runs on
- [SDL](https://libsdl.org): windowing, graphics, audio and input
- [lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) (MIT) and
  [DXVK](https://github.com/doitsujin/dxvk)'s shader compiler (zlib), through
  `lib/lsfg` from BattleTanx: Global Assault Recompiled: Lossless Scaling
  frame generation
- [SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers) (MIT), for
  building that
- [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause):
  custom GPU drivers on Android
- [glslang](https://github.com/KhronosGroup/glslang) (BSD and others, see
  its license): the game's vertex programs made into GPU shaders of their own
- Mad Dash Recomp, Orta Recomp and Concept of Love: the code this port
  started from

## License

The project's own code is GPL-3.0; see [COPYING](COPYING). Third-party
licenses are in [licenses/](licenses). The game itself remains the property
of its rights holders.
