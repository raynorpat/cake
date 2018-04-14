project(refresh)

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

if (NOT WIN_UWP)
	# glew
	find_package(GLEW)
	if (GLEW_FOUND)
		include_directories(${GLEW_INCLUDE_DIRS})
		set(GLEW_LIBRARY ${GLEW_LIBRARIES})
	else (GLEW_FOUND)
		include_directories("../libs/glew/include")
		set(GLEW_LIBRARY "" )
		add_definitions(-DGLEW_STATIC)
	endif (GLEW_FOUND)
	if (NOT GLEW_FOUND)
		set(GLEW_INCLUDES ../libs/glew/include/GL/glew.h)
		set(GLEW_SOURCES ../libs/glew/src/glew.c)
		if(WIN32)
			set(GLEW_INCLUDES ${GLEW_INCLUDES} ../libs/glew/include/GL/wglew.h)
		else(WIN32)
			set(GLEW_INCLUDES ${GLEW_INCLUDES} ../libs/glew/include/GL/glxew.h)
		endif(WIN32)
	else (NOT GLEW_FOUND)
		set(GLEW_INCLUDES "")
		set(GLEW_SOURCES "")
	endif (NOT GLEW_FOUND)
	source_group("libs\\glew" FILES ${GLEW_INCLUDES})
	source_group("libs\\glew" FILES ${GLEW_SOURCES})
endif()

# sdl
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH ../libs/SDL2)
endif()
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
set(SDLx_LIBRARY ${SDL2_LIBRARY})


# refresh sources
set(RENDERER_INCLUDES
	../refresh/ref_public.h
	../refresh/vid_public.h
	)
set(RENDERER_SOURCES
	../refresh/matrix.c
	../refresh/ref.c
	)
source_group("refresh" FILES ${RENDERER_INCLUDES})
source_group("refresh" FILES ${RENDERER_SOURCES})

if (NOT WIN_UWP)
# OpenGL refresh backend sources
set(RENDERER_GL_INCLUDES
	../refresh/gl/gl_model.h
	../refresh/gl/gl_local.h
	../refresh/gl/gl_state_dsa.h
	)
set(RENDERER_GL_SOURCES
	../refresh/gl/gl_beam.c
	../refresh/gl/gl_draw.c
	../refresh/gl/gl_framebuffer.c
	../refresh/gl/gl_fragment.c
	../refresh/gl/gl_font.c
	../refresh/gl/gl_image.c
	../refresh/gl/gl_light.c
	../refresh/gl/gl_mesh.c
	../refresh/gl/gl_model.c
	../refresh/gl/gl_nullmodel.c
	../refresh/gl/gl_part.c
	../refresh/gl/gl_postprocess.c
	../refresh/gl/gl_rmain.c
	../refresh/gl/gl_rmisc.c
	../refresh/gl/gl_rsurf.c
	../refresh/gl/gl_sky.c
	../refresh/gl/gl_sprite.c
	../refresh/gl/gl_state.c
	../refresh/gl/gl_warp.c
	../refresh/gl/gl_state_dsa.cpp
	../refresh/vid/vid_gl.c
	)
source_group("refresh\\gl" FILES ${RENDERER_GL_INCLUDES})
source_group("refresh\\gl" FILES ${RENDERER_GL_SOURCES})
endif()

# video backend sources
set(RENDERER_VID_INCLUDES
	../refresh/vid_public.h
	)
set(RENDERER_VID_SOURCES
	../refresh/vid/vid.c
	)
source_group("refresh\\vid" FILES ${RENDERER_VID_INCLUDES})
source_group("refresh\\vid" FILES ${RENDERER_VID_SOURCES})


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

# build the refresh library
add_library(refresh STATIC ${GLEW_INCLUDES}
	${GLEW_SOURCES}
	${FREETYPE_INCLUDES}
	${FREETYPE_SOURCES}
	${RENDERER_INCLUDES}
	${RENDERER_SOURCES}
	${RENDERER_GL_INCLUDES}
	${RENDERER_GL_SOURCES}
	${RENDERER_VID_INCLUDES}
	${RENDERER_VID_SOURCES}
	)
