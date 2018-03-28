# Cake
![Cake](https://github.com/raynorpat/cake/raw/master/docs/scrnshot.png)

**The project is hosted at:** https://github.com/raynorpat
**Report bugs here:** https://github.com/raynorpat/cake/issues

## Overview

Cake is a modified Quake 2 based game engine built on top of the Quake 2 source code released by id Software.

Some of the major features currently implemented are:

  * Easy to use modern build system via CMake
  * SDL 2 backend
  * OpenAL audio backend
  * Background Music support (tracker xm/s3m/it/mod, mp3, and even wav)
  * RoQ cinematic video encoder/decoder
  * Re-written OpenGL 3.3 renderer built for speed and a retro software look
  * HTTP/FTP download redirection (using cURL)
  * Integrated content creation tools
  * Master server tool
  * Many, many, many bug fixes

## Downloads

Pre-built releases of _Cake_ are available at: https://github.com/raynorpat/cake/releases

## Compiling

_Cake_ builds with CMake. If you've never tried it, please do so, it rocks. We require at least version 3.0.

The main dependency to build _Cake_ is SDL2, which is provided in library format for MSVC builds, all others will need to grab it
from source or from their distribution.

Other dependencies include glew, OpenAL, libcurl, libmad, libmodplug, and zlib.
We provide all dependencies for building everything else in src_main/libs if CMake doesn't find one.

There are various scripts in the src_main directory that will give you a way to build if CMake is installed on your machine. These scripts range from
regular old GNU makefiles to XCode projects as well as MSVC 2010 all the way through 2017 32bit and 64bit variants.

## Installing

To have a working game, you must install the game data into the baseq2 folder. 
All that is required is a copy of the full retail version of Quake 2. That is pak0.pak, pak1.pak, and pak2.pak.

_Cake_ requires SDL2 to be installed or available on your system to function.
If on Windows, that usually just requires putting SDL2.dll next to the main engine exe.

## Contributing

Please send all patches as a GitHub pull request, that will make everything easier to track.

## Copyright, & Permissions

Copyright (C) 1996-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify it under 
the terms of the GNU General Public License as published by the Free Software 
Foundation; either version 2 of the License, or (at your option) any later 
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY 
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
PARTICULAR PURPOSE.

See the LICENSE file for more details.

## Credits

Maintainers

  * Pat 'raynorpat' Raynor <raynorpat@gmail.com>
  
Contributions from

  * Ryan C. Gordon <icculus@icculus.org> - initial sdl 1.2 quake2 port - see http://www.icculus.org/quake2/
  * Stuart Dalton <badcdev@gmail.com> - ioquake3 sound codecs system
  * O. Sezer <sezero@users.sourceforge.net> - uHexen II sound codecs system
  * yquake2 and KMQ2 projects - Save game system
  * FFmpeg project - RoQ video encoder
  * Frank Sapone - master server - see https://bitbucket.org/maraakate/gsmaster
  * Deniz Sezen - bug fixes