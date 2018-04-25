project(renderer)

# freetype
if (USE_FREETYPE)
	add_definitions(-DUSE_FREETYPE)
	find_package(FREETYPE)
	if (FREETYPE_FOUND)
		include_directories(${FREETYPE_INCLUDE_DIRS})
		set(FREETYPE_INCLUDES "")
		set(FREETYPE_SOURCES "")
	else (FREETYPE_FOUND)
		include_directories("../libs/freetype/include")
		add_definitions(-DFT2_BUILD_LIBRARY)
		file(GLOB FREETYPE_INCLUDES ../libs/freetype/include/*.h ../libs/freetype/include/freetype/*.h ../libs/freetype/include/freetype/config/*.h)
		set(FREETYPE_SOURCES ../libs/freetype/src/autofit/autofit.c
							 ../libs/freetype/src/bdf/bdf.c
							 ../libs/freetype/src/cff/cff.c
							 ../libs/freetype/src/base/ftbase.c
							 ../libs/freetype/src/base/ftbitmap.c
							 ../libs/freetype/src/cache/ftcache.c
							 ../libs/freetype/src/base/ftdebug.c
							 ../libs/freetype/src/base/ftgasp.c
							 ../libs/freetype/src/base/ftglyph.c
							 ../libs/freetype/src/gzip/ftgzip.c
							 ../libs/freetype/src/base/ftinit.c
							 ../libs/freetype/src/lzw/ftlzw.c
							 ../libs/freetype/src/base/ftstroke.c
							 ../libs/freetype/src/base/ftsystem.c
							 ../libs/freetype/src/smooth/smooth.c
							 ../libs/freetype/src/base/ftbbox.c
							 ../libs/freetype/src/base/ftmm.c
							 ../libs/freetype/src/base/ftpfr.c
							 ../libs/freetype/src/base/ftsynth.c
							 ../libs/freetype/src/base/fttype1.c
							 ../libs/freetype/src/base/ftwinfnt.c
							 ../libs/freetype/src/pcf/pcf.c
							 ../libs/freetype/src/pfr/pfr.c
							 ../libs/freetype/src/psaux/psaux.c
							 ../libs/freetype/src/pshinter/pshinter.c
							 ../libs/freetype/src/psnames/psmodule.c
							 ../libs/freetype/src/raster/raster.c
							 ../libs/freetype/src/sfnt/sfnt.c
							 ../libs/freetype/src/truetype/truetype.c
							 ../libs/freetype/src/type1/type1.c
							 ../libs/freetype/src/cid/type1cid.c
							 ../libs/freetype/src/type42/type42.c
							 ../libs/freetype/src/winfonts/winfnt.c)
	endif (FREETYPE_FOUND)
	source_group("libs\\freetype" FILES ${FREETYPE_INCLUDES})
	source_group("libs\\freetype" FILES ${FREETYPE_SOURCES})
endif()

# sdl
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH ../libs/SDL2)
endif()
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
set(SDLx_LIBRARY ${SDL2_LIBRARY})


# renderer sources
set(RENDERER_INCLUDES
	../refresh/r_model.h
	../refresh/r_local.h
	../refresh/ref_public.h
	../refresh/vid_public.h
	)
set(RENDERER_SOURCES
	../refresh/gfx.c
	../refresh/r_beam.c
	../refresh/r_draw.c
	../refresh/r_fragment.c
	../refresh/r_font.c
	../refresh/r_image.c
	../refresh/r_light.c
	../refresh/r_matrix.c
	../refresh/r_mesh.c
	../refresh/r_model.c
	../refresh/r_nullmodel.c
	../refresh/r_part.c
	../refresh/r_postprocess.c
	../refresh/r_main.c
	../refresh/r_misc.c
	../refresh/r_surf.c
	../refresh/r_sky.c
	../refresh/r_sprite.c
	../refresh/r_warp.c
	)
source_group("renderer" FILES ${RENDERER_INCLUDES})
source_group("renderer" FILES ${RENDERER_SOURCES})

# OpenGL backend sources
set(RENDERER_GL_INCLUDES
	../refresh/gl_gl.h
	)
set(RENDERER_GL_SOURCES
	../refresh/gl_framebuffer.c
	../refresh/gl_state.c
	../refresh/gl_gl.c
	../refresh/gl_gl_dsa.c
	../refresh/gl_vid.c
	)
source_group("renderer\\gl" FILES ${RENDERER_GL_INCLUDES})
source_group("renderer\\gl" FILES ${RENDERER_GL_SOURCES})

#
# Refresh library
#

# set include directories
include_directories(
			.
			../engine
			../shared
			../libs/stb
			../libs/libcurl/include # because client.h includes curl.h
            )

# build the renderer library
add_library(renderer STATIC 
	${FREETYPE_INCLUDES}
	${FREETYPE_SOURCES}
	${RENDERER_INCLUDES}
	${RENDERER_SOURCES}
	${RENDERER_GL_INCLUDES}
	${RENDERER_GL_SOURCES}
	)
