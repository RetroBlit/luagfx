LuaGFX is a Lua 5.1.5 port for the 16 bit ELKS OS with an integrated small game engine and multiple graphics backends. It is designed for old retro computers such as 286, 386, 486, and later machines. A future MS-DOS port is also being considered.

## Supported backends

* VGA mode X - the primary and best-optimized backend. It is suitable for games using sprites, tiles, tilemaps, page flipping, and background restore. It is less suitable for random immediate-mode VGA drawing, such as many circles or arbitrary pixel-heavy graphics.
* Nano-X - an optimized but secondary backend. It is mainly intended as a compatibility backend for running games under Nano-X. Performance is expected to be lower than the Mode X backend.
* Direct VGA / Mode 13h — planned backend for simple linear-framebuffer drawing. It is expected to be useful for random pixel graphics, effects, circles, and simple demos. It is not the primary target for sprite/tile games because it lacks the page-flipping and background-page model used by the Mode X backend.

The API is the same for all graphics backends, but performance and behavior may differ. It is best to choose the primary backend in advance, depending on the type of graphics your game needs and whether Nano-X compatibility is important.

## Build notes

LuaGFX is built with OpenWatcom C for ELKS.

- Default ELKS build: `make -f Makefile.elks`
- Nano-X graphics backend: `make USE_NANOX_BACKEND=1 NANOX_DIR=/path/to/microwindows/src`

## Acknowledgements

This project is based on the Lua project and the work of Rafael Diniz port of Lua to ELKS.
  
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
