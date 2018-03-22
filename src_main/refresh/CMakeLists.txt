project(refresh)

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

# Software refresh backend sources
set(RENDERER_SOFT_INCLUDES
	../refresh/soft/adivtab.h
	../refresh/soft/anorms.h
	../refresh/soft/r_model.h
	../refresh/soft/r_local.h
	)
set(RENDERER_SOFT_SOURCES
	../refresh/soft/r_aclip.c
	../refresh/soft/r_alias.c
	../refresh/soft/r_bsp.c
	../refresh/soft/r_draw.c
	../refresh/soft/r_edge.c
	../refresh/soft/r_image.c
	../refresh/soft/r_light.c
	../refresh/soft/r_main.c
	../refresh/soft/r_misc.c
	../refresh/soft/r_model.c
	../refresh/soft/r_part.c
	../refresh/soft/r_poly.c
	../refresh/soft/r_polyse.c
	../refresh/soft/r_rast.c
	../refresh/soft/r_scan.c
	../refresh/soft/r_sprite.c
	../refresh/soft/r_surf.c
	)
source_group("refresh\\soft" FILES ${RENDERER_SOFT_INCLUDES})
source_group("refresh\\soft" FILES ${RENDERER_SOFT_SOURCES})

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
	${RENDERER_INCLUDES}
	${RENDERER_SOURCES}
	${RENDERER_SOFT_INCLUDES}
	${RENDERER_SOFT_SOURCES}
	${RENDERER_GL_INCLUDES}
	${RENDERER_GL_SOURCES}
	${RENDERER_VID_INCLUDES}
	${RENDERER_VID_SOURCES}
	)
