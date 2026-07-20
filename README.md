LuaGFX is a Lua 5.1.5 port for the 16 bit [ELKS OS](https://github.com/ghaerr/elks) with an integrated small game engine with multiple graphics backends. It is designed for old retro computers such as 286, 386, 486, and later machines. A future MS-DOS port is also being considered. Because of its Nano-X backend, many more platforms can potentially be supported such as Linux X11, macOS SDL2 or X11, FreeBSD X11, Solaris/Illumos X11, Windows MinGW. Alternatively ELKS and LuaGFX can be used in a x86 emulator such as DOSBox for Solaris or OpenVMS/Alpha for example.

<img src="https://github.com/user-attachments/assets/8853e5e9-532c-43e5-970c-b6a05f2d4c2a" width="49%" />  
<img src="https://github.com/user-attachments/assets/bcc8d0cc-2152-4363-bd5f-c02399083dcf" width="49%" />

Left image represents using the VGA mode X backend and the right image the Nano-X backend. Game resolution is 320 x 240 for both. Same code robotjmp.lua is used. Left image is 256 colors, while right image is limited to 16 colors (but higher total resolution).

## Supported graphics backends

* VGA mode X - heavily optimized. It is suitable for games using sprites, tiles, tilemaps, page flipping, and background restore. It is less suitable for random immediate-mode VGA drawing, such as many circles or arbitrary pixel-heavy graphics.
* Nano-X - heavily optimized. Performance might be lower than the Mode X backend, because of the Nano-X extra layer, which is client-server based. This mode reduces the need for adding new backends where Nano-X and Lua are ported.
* Direct VGA / Mode 13h — planned backend for simple linear-framebuffer drawing. It is expected to be useful for random pixel graphics, effects, circles, and simple demos. It is not the primary target for sprite/tile games because it lacks native page-flipping and background-page model used by the Mode X backend.

The API is the same for all graphics backends, but performance will differ. It is best to choose a primary backend in advance, depending on the type of graphics your game needs and whether Nano-X compatibility is important. Please note that start-up time can be long. Expect between 20s on a fast machine and 2 minutes on a 8086 at 8 Mhz on heavy game. Call `gfx.compile_sprite(id)` after defining the sprite, during script initialization, to enable faster rendering.

## Documentation

Check the [wiki](https://github.com/RetroBlit/luagfx/wiki).

## How to build?

LuaGFX (a clone of Lua) is built with OpenWatcom C for ELKS. You need two components:
* clone [ELKS](https://github.com/ghaerr/elks) on Linux or WSL for Windows
* for OpenWatcom C on Linux follow [here](https://github.com/ghaerr/elks/wiki/Using-OpenWatcom-C-with-ELKS). Once OWC is installed, you need to compile ELKS' libc for large model support `make -f watcom.mk MODEL=l`. This large model libc will be used for the LuaGFX compilation.

Compile commands:
* the default ELKS build with VGA mode X backend: `make -f Makefile.elks`
* Nano-X graphics backend build: `make USE_NANOX_BACKEND=1 NANOX_DIR=/path/to/microwindows/src`

## Acknowledgements

LuaGFX is based on the Lua project and the work of Rafael Diniz port of Lua to ELKS OS.
  
# README for Lua 5.1

See INSTALL for installation instructions.
See HISTORY for a summary of changes since the last released version.

## What is Lua?

  Lua is a powerful, light-weight programming language designed for extending
  applications. Lua is also frequently used as a general-purpose, stand-alone
  language. Lua is free software.

  For complete information, visit Lua's web site at http://www.lua.org/ .
  For an executive summary, see http://www.lua.org/about.html .

  Lua has been used in many different projects around the world.
  For a short list, see http://www.lua.org/uses.html .

## Availability

  Lua is freely available for both academic and commercial purposes.
  See COPYRIGHT and http://www.lua.org/license.html for details.
  Lua can be downloaded at http://www.lua.org/download.html .

## Installation

  Lua is implemented in pure ANSI C, and compiles unmodified in all known
  platforms that have an ANSI C compiler. In most Unix-like platforms, simply
  do "make" with a suitable target. See INSTALL for detailed instructions.

## Origin

  Lua is developed at Lua.org, a laboratory of the Department of Computer
  Science of PUC-Rio (the Pontifical Catholic University of Rio de Janeiro
  in Brazil).
  For more information about the authors, see http://www.lua.org/authors.html .
