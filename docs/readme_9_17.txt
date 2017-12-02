Quake II  9/17/97 code update
-----------------------------

per-face attributes
-------------------
A big change is extended support for per-face attributes and the movement of textures to seperate files.

A given texture has three ints associated with it:  contents, flags, and value.

The contents bits on the faces of a brush determine the volume contents.  Qbsp3 will report a warning if all sides do not have identical contents flags.  CONTENTS_SOLID is implicit if nothing else is specified.  If one or more faces have translucency, the contents will be forced to CONTENTS_WINDOW if it would otherwise be solid.

The flags bits determine per-face attributes for utilities (light emiting), rendering (translucency, flowing, warping), and server interaction (slippery).

The value field is currently only used to determine the intensity of light emiting surfaces, but it could be used for other purposes on other flag types.

When grabbing textures with qdata, you can specify default flags, contents, and values.

QuakeEd maintains a "curent surface state" that consists of a texture name, texture placement info (shifts, scales, and rotate), and flags/contents/value.

Whenever you draw a new brush, the current surface state is applied to all new faces.  If you have any brushes or a face selected, any change to the surface state will cause all selected faces to be immediately updated.  QuakeEd now has the ability to select a single face on a brush by ctrl-shift-clicking in the camera view.

The full surface state can be changed explicitly with the surface inspector (press 'S').  Clicking on a texture in the texture view will clear the surface placement information, set the texture name, and set the contents/flags/value to the defaults for that texture.  Middle clicking on a face in the camera view will grab the complete surface state from the face clicked on.


Surface attributes
------------------
SURF_TRANS33
SURF_TRANS66
Partially opaque surfaces.  Translucent surfaces will allways be fullbright (no lightmaps).

SURF_WARPING
Warping can be applied to any 64*64 texture.  A warping texture will allways be fullbright (no lightmaps).

SURF_SKY
A sky texture will never actually be drawn.  It simply marks space where the sky environment map backdrop will show through.

SURF_FLOWING
Warping textures can be made to flow in the direction of their rotation.

SURF_NODRAW
A hint to the game that this surface's texture will never actually be used to draw anything.  Origin brushes, clip brushes, trigger brushes, sky brushes, etc should all have this set so a texture is not created at runtime for them. (not implemented yet)




Textures can be freely mixed from any texture directory in a single map file now.  Our current texture directories contain some duplication of images because of the previous wad structure, but new projects should just reference common textures from a single directory, rather than duplicating them. 

The .map source file format has changed slightly.  There is no more "_wad" keyword in the world entity, and all texture names now include a directory as well as a base texture.  Surfaces that have had flags, values, or contents modified in the surface editor will have three additional numbers on each brush line.

A tool has been included that will convert older maps to the new format.  It looks for the "_wad" keyword to determine a texture directory, then goes through every surface and replaces all the texture names with a lowercased version prepended by the texture directory name.  The "_wad" key is removed, so a map cannot be converted twice.

Detail / structural brushes
---------------------------
Brushes in the editor can now be flagged as either "detail" or "structural".

The basic idea is that you should make all of the brushes that aren't major occluders into detail brushes.  This lets the vis program ignore all of the little protrusions that never really block sight.  Because of the factorial order of the vis algorithm, it is pretty easy to get a 10x speedup in vis time by making the non-essential brushes detail.

This also gives a method to fight some of the errors that still occur in bsps.  If polygons are dissapearing in the game, but they come back with "r_novis 1" set, then there is a vis error.  This can usually be fixed by making more of the brushes between they problem viewpoint and the problem polygon into detail brushes.

If the polygons are gone at all times, then it is a bsp problem, which we still don't have a great answer for.


Other changes that come to mind
-------------------------------
Quake2 is now using VC 5.0 with service packs one and two.  We are still finding some code generation problems with release builds.  All the utilities still have VC 4.2 projects.

Quake2 compiles and runs on alpha AXP NT systems.

Memory use is still very unoptimized.  Don't expect it to work on 16 meg machines right now.

The game logic files are now in a real dll instead of compiled in.  The dll is not really in a form fo user level hacking yet, because the server still shares asusmptions about structure sizes.

The funtionality of qlumpy.exe is now merged into qdata.exe.

For model frame grabbing with qdata, the frame "run1" will search for either "run1.tri" or "run.1".  This allows alias/wavefront to save out sequenced animations with a single step.

The qbsp3/qvis3/qrad3 utilities have been subprojected into a single bsp.mdp project file.

Texture animation is currently unfinished.

Network code is functional, but there are no player model animations yet, and the player lifecycle isn't coded yet, so deathmatch isn't really working.  The network packet size is completely unoptimized.  No deltas at all.  Don't try to connect over anything but a lan.

The framework for cinematics is present, but the frames are not compressed yet.

The framework for savegames is present, but nothing is actually saved yet.


Utility notes
-------------

qdata.exe
Processes /quake2/baseq2/scripts/*.qdt files to grab all of the data elements for the game (2D images, sprites, models, miptextures, colormaps, etc.) except .bsp files.  Release pak files are also built by qdata.

bspinfo3.exe
Dumps statistics on .bsp files.

qbsp3.exe
There are still some problems with non-translucent water, and some areas can still lose polygons.  Release builds from vc 4.2 seem to have problems on some maps.

qvis3.exe
There are still some vis failures (actually, I think they are bsp portalization errors), but you can usually fix them with detail brushes now.

qrad3.exe
There are currently light bleeding bugs.  They should be fixed by release time.

texpaint.exe

qe4.exe
There is now a "region selected brushes" option, which supresses everything not selected, allowing you to more easily carve pieces away from brushes too near supporting structures.

convert
This is the converter that updates .map files from quake 1 to the quake 2 format (basically just massaging texture names).  Christian wrote it in visual basic, so direct any questions to xian@idsoftware.com.

Game directory structure
------------------------
/baseq2/demos
All recorded or played back demos go in this directory.

/baseq2/gfx
Only image source files for qdata are stored here.  No derived or referenced files for any other utility or the game are contained here.

/baseq2/env
The derived sky box images referenced by the game are stored here.  24 bit tga files for gl, and uniformly palette mapped 8 bit pcx files.

/baseq2/maps
Both source and derived files for qe4/qbsp3/qvis3/qrad3 are stored here.

/baseq2/models
Both source and derived files for models processed by texpaint and qdata are stored here.
A model file is allways "tris.md2", and the skins are often "skin.pcx", so the directory path is significant.

/baseq2/pics
Derived files for simple 2d images grabbed by qdata.  All are standard pcx files that have been cropped out of larger images.

/baseq2/scripts
Source script files for qdata, and the location of quake.qe4, the editor configuration file.

/baseq2/scrnshot
Screen shots are stored here.  The software renderer creates .pcx files, the gl renderer creates .tga files.

/baseq2/sound

/baseq2/sprites
Derived files for sprites grabbed by qdata go here.  Cropped pcx files for images, and .sp2 files for model frame references.

/baseq2/textures
Derived .wal files grabbed by qdata go here.  They are referenced by both qe4 and quake2.  The directory path under textures is significant, so you can hierarchially organize textures.

/baseq2/video
Currently the video is just linearly numbered pcx files, but they will be packed and interleaved with sound before release.

running quake
-------------
cd /quake2
quake2

This will begin with the logo cinematic, then cycle into demos.  The exact sequencing is all controlled by alias commands in default.cfg.

Hitting the console key will stop any demos and drop to a full screen console.  Hitting it again will restart the demo loop.

Just add a + after quake2 to get it to start up and drop to the console without running any cinematics or demos.

There are very few "dash" parameters (checked specifically for on the command line)left in quake 2.  Almost all configuration variables are now cvars, so you can inspect them or add them to config files.

The change that has made this possible is the addition of the "set" command, which allows you to set a cvar variable even if it hasn't been defined yet.

For instance, "quake2 +set vid_fullscreen 1" will cause the vid_fullscreen cvar to be defined and set to 1 even before the video subsystem gets initialized.  When it inits, it will find the value allready set, and use that instead of the default value.

Quake2 scans the command line parms twice.  The first time, it only pulls out "set" commands.  This happens after default.cfg has been executed, but before any of the IO systems are initialized.  After all the systems are completely initialized, all remaining + command line strings are added and executed.

quake2 +set vid_fullscreen 0 +map base_ec +set snd_khz 22

This would start quake2 up, execute default.cfg, then set vid_fullscreen and snd_khz, then initialize all the rest of the game, then run the map command.

There are only a couple dash parameters left:  -basedir, -cddir, and -game.

-basedir <path>
This specifies a directory to use other than the current directory as the base for all gamedirs.

-cddir <path>
If a cddir is specified, that path will be checked for files that aren't found in the current basedir hierarchy.  This is to allow partial or minimal installs to the local harddrive with most of the data residing on a cd drive.

-game <directoryname>

During development, a common command line might be:

quake2 -basedir c:\quake2 -cddir g:\quake2

That would let you have your config file saved to your local drive, but reference most files on a shared network.  You could also put modified testing versions of files on your local drive and they would take precedence.


regenerating all derived data
-----------------------------
All derived data is allready present on the cd, but here is the regeneration process for completeness:

It is important that the files be in a "quake2" directory, because the
utilities scan for that to identify the game directory.

cd /quake2/baseq2/scripts
qdata *.qdt

This will take a long time.

cd /quake2/baseq2/maps
for %i in (*.map) win32bsp %i

This will take a very long time.


Creating a Quake II addon
-------------------------
Create a "mygame" (or whatever) directory under quake 2.
Create a "mygame/maps" directory.
Create a "mygame/scripts" directory.
Create a "mygame/pics" directory.

copy baseq2/pics/colormap.pcx to mygame/pics/colormap.pcx
The editor needs this file to get a palette for the textures, and qrad needs it to calculate average texture reflectivity.

copy baseq2/scripts/quake.qe4 mygame/scripts

Edit mygame/scripts/quake.qe4 as apropriate.  At a minimum, you will need to change "basepath" and "remotebasepath".  Depending on how you want to store your textures, you may leave "texturepath" pointing to baseq2, or generate your own directory hierarchy.  Unless you have a remote bsp server set up, set "rshcmd" to "" and make sure /quake2/bin_nt is in your search path.

cd mygame

qe4
create a simple map with an "info_playerstart".
save it as "mymap"
select bsp->fullvis and wait for it to complete
exit

cd ..
quake2 -game mygame +map mymap

