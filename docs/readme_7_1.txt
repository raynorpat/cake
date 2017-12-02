Quake2 7/1 code dump
--------------------
Create a 'quake2' directory, and copy the entire contents of the cd into it.  This includes all of our source data (500+ megs of it...) for reference.  You can delete most of it after you are familiar with it.

Add /quake2/bin_nt to your search path.

exercising command line tools
-----------------------------
cd /quake2/baseq2/scripts

qlumpy e?m?.ls
// grabs all the wall textures for all the episodes,
// generates /quake2/gfx/*.wad files read by qe4 and qbsp3
// this takes significant time due to mip mapping, error
// diffusion, and palette mapping.
// This will soon be integrated with qdata, and I hope to move to
// discrete image files instead of .wad files.

qdata *.qdt
// grabs all the images, models, and sprites
// All images are grabbed off of full screen art pages (usually .lbm for us,
// but it also reads .pcx) and saved off as cropped .pcx files, which quake2
// reads directly.  We intend to move towards editing directly on the
// cropped images and only refering to them in the .qdt scripts, but right
// now you should allways draw on seperate pages and ignore the cropped
// images.
// elaborate: -only option, model header file generation


qdata -pak /quake2/baseq2/pak0.pak *.qdt
// combines all output files (and files/directories specifically included)
// into a single pak file for distribution purposes.  Pak files are also
// handy for just making the game start up faster during development.
// You can also do qdata "-release /atest/baseq2" to cause the files to
// just be copied to another tree instead of into a pak file.

cd /quake2/baseq2/maps

qbsp3 jail_e3
qvis3 jail_e3
qrad3 jail_e3
// takes jail_e3.map and build jail_e3.bsp

bspinfo3 jail_e3
// dumps information on jail_e3.bsp


texpaint
--------
cd /quake2/baseq2/models/tank
texpaint skin.lbm

REQUIRES A FULL 24 BIT OPENGL if you are going to use the painting-on-model features.  It can be used as a model viewer on any opengl system.

Displays skin.lbm on the base.tri model.  You can load a different skin
(pain.lbm, for example), or load another .tri file to see other poses.

A three button mouse is required for proper operation.

The left button paints with the current color in either the skin window
or the camera window, or selects colors in the palette view.  Ctrl-left
click picks up the color clicked on in any view, making it current.

Right button dragging slides the object or skin around.  Ctrl-right drag
to move in or out on the object or skin.

Middle button dragging rotates the object in the camera view.  Ctrl-middle
drag to change the roll angle.



QuakeEd 4
---------
cd /quake2/baseq2
qe4

Reads the /quake2/baseq2/quake.qe3 configuration file, which contains the following key / value pairs:

"basepath" : the path to the game directory on the local machine

"remotebasepath" : the path to the game directory for bsp commands.  This may be different if the bsp machine runs unix, or has drives mounted differently.

"wads" : a space seperated list of all the texture wad files.  These will be added to the textures menu.

"wadpath" : an optional key that lets you get the wad files from someplace other than the basepath.  We use this so that people working over ISDN can avoid the texture load times, but still have map data sent over the network so the bsp machine can read it.

"autosave" : specifies the full path used to autosave the current map every five minutes.  Should not be on a shared drive, or multiple editors will conflict.

"bsp_*" : any keys that begin with "bsp_" will be added to the bsp menu.  Any $ characters will be expanded to be "remotebasepath" + current map name.  @ characters will be changed to quotes, but that currently isn't needed in any of our commands.  If you want to bsp on your local machine, remove the "rsh seera" strings from every bsp command, and set "remotebasepath" to the same thing as "basepath".


Don't bother running this on a system without OpenGL acceleration.  It works (slowly), but you'll be sad.  Permedia II boards with 8 meg are only $200...

We don't have any documentation on using the editor.  Sorry.

Every save or bsp, the editor renames the current .map to .bak, then writes all brushes and entities to the .map fuke.  If the editor is in region mode, it will also produce a .reg files containing only the regioned brushes.  (currently, it only does this when you actually run a bsp command, not if you just do a file->save).


Development notes
-----------------
Eventually, your development work should be moved to a seperate /quake2/mygame hierarchy, and accessed with quake2 -game mygame, but for now it may be easier to just work in the baseq2 hierarchy (-game hasn't been tested with Q2, I'm not sure it works at the moment).

The current qbsp3 creates some maps that can take far too long to qvis.  I'm working on the problem, but you may just want to use FastVis (qvis3 -fast) whenever a map starts taking too long.  That won't give you an accurate speed estimate, but it will let you keep working until I get the tools improved.


File descriptions
-----------------
/quake2/quake2.exe
The main quake2 executable.

/quake2/ref_gl.dll
The opengl renderer dll, dynamically loaded by quake2.exe

/quake2/ref_soft.dll
The software renderer dll, dyaamically loaded by quake2.exe.

/quake2/fxmemmap.vxd
3dfx glide vxd, should be put in windows/system if glide isn't allready installed.

/quake2/glide2x.dll
3dfx glide dll, used by the 3dfx opengl32.dll

/quake2/opengl32.dll
3dfx specific gl mini driver.  If you have a real opengl system (intergraph, etc) you should delete this file, because it will prevent the system dll from functioning.  We are currently implementing a manual library load to avoid this problem.

/quake2/code/
Source to build quake2.exe, ref_soft.dll, and ref_gl.dll.  Rhapsody code exists, but is not up to date.  You must have masm installed to rebuild the executables.  ml.exe is included in /bin_nt, in case you don't have it.

/quake2/code/game/
These files are currently just compiled into quake2.exe, but soon they will be living in a seperate .dll that you will be free to modify.

/quake2/bin_nt/
Win-32 compiled versions of all the utilities.

/quake2/bin_nt/bspinfo3.exe
Dumps information on a bsp file.

/quake2/bin_nt/qbsp3.exe
Converts a .map file into a .bsp file which can be used by the game.

/quake2/bin_nt/qvis3.exe
Builds potentially visible set information for a .bsp to accelerate rendering and qrad calculations.

/quake2/bin_nt/qrad3.exe
Builds lightmaps for a .bsp file.  If the PVS has not been generated, only direct lighting is considered.  Otherwise, a radiosity solution is done.

/quake2/bin_nt/qlumpy.exe
Currently only used for grabbing textures to be used by qe4.exe and qbsp3.exe.  This will probably be merged with qdata soon.  Reads .ls files from /baseq2/scripts.

/quake2/bin_nt/qdata.exe
Reads .qdt files from /baseq2/scripts and generates cropped pcx files for 2d images, .md2 files for models, and .sp2 files for sprites.

/quake2/bin_nt/texpaint.exe
Interactive OpenGL program for creating and painting on model skins.  Reads alias .tri files and reads / writes .lbm images.

/quake2/bin_nt/qe4.exe
The map editor.  Should be launched with the current directory set to /quake2/baseq2.

/quake2/utils3/
Source for utilities in /bin_nt/.

/quake2/utils3/common/
Common code used by all the utilities.  No executable generated.

/quake2/baseq2/
Data for Quake2.  Both source and derived data are held in this hierarchy.

/quake2/baseq2/quake.qe3
Configuration file for qe4.  We will be moving this to the scripts dir soon.

/quake2/baseq2/scripts/common.ls
Wall graphics $included by other .ls files.

/quake2/baseq2/scripts/e?u?.ls
Qlumpy script files to create texture wads.

/quake2/baseq2/scripts/*.qdt
Qdata script files to grab models, sprites, and images.

/quake2/baseq2/gfx/env
Environment map images for outdoor skies.  The naming convention is the output from 3dstudio's environment map option: six characters, followed by "bk", "ft", "lf", "rt", "up", or "dn".  The software renderer will use .pcx images, and the gl renderer uses .tga images.  The pcx images need to be generated together by a special utility to avoid palette quantization seams between the images.  The software sky doesn't work yet, so that utility is not included.  It will be merged into qdata by next dump.
The default sky is "sky1", but you can set values in the world entity to cause a map to have a different sky:
"sky" "space1"		// the base images
"skyaxis" "1 1 1"	// vector to rotate around for space scenes
"skyrotate" "4"	// degrees / second rotation speed

/quake2/baseq2/pics
Cropped 2d images generated by qdata, and loaded by quake2.  Mostly 2D interface components, like the console and status items.

/quake2/baseq2/sprites
Sprite data files and cropped images generated by qdata, and loaded by quake2.

/quake2/baseq2/sound
For shipping, we will include both sound_22_16 and sound_11_8 directories, which can be dynamically selected.

/quake2/baseq2/glquake
This directory was created by the gl renderer to hold cached model meshes, but it is no longer needed with the new model format.   It will be gone in the next dump.



