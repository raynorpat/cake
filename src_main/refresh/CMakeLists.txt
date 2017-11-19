project(refresh)

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

# sdl
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH ../libs/SDL2)
endif()
if(SDL2)
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIR})
	set(SDLx_LIBRARY ${SDL2_LIBRARY})
else()
	find_package(SDL REQUIRED)
	include_directories(${SDL_INCLUDE_DIR})
	set(SDLx_LIBRARY ${SDL_LIBRARY})
endif()

# refresh sources
set(RENDERER_INCLUDES
	../refresh/ref_public.h
	../refresh/vid_public.h
	)	
set(RENDERER_SOURCES
	../refresh/matrix.c
	)
source_group("refresh" FILES ${RENDERER_INCLUDES})
source_group("refresh" FILES ${RENDERER_SOURCES})

# OpenGL refresh backend sources
set(RENDERER_GL_INCLUDES
	../refresh/gl/gl_model.h
	../refresh/gl/gl_local.h
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
	)
source_group("refresh\\gl" FILES ${RENDERER_GL_INCLUDES})
source_group("refresh\\gl" FILES ${RENDERER_GL_SOURCES})

# video backend sources
set(RENDERER_VID_INCLUDES
	../refresh/vid_public.h
	)	
set(RENDERER_VID_SOURCES
	../refresh/vid/vid.c
	../refresh/vid/vid_gl.c
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
            )

# build the refresh library
add_library(refresh STATIC ${GLEW_INCLUDES} ${GLEW_SOURCES} ${RENDERER_INCLUDES} ${RENDERER_SOURCES} ${RENDERER_GL_INCLUDES} ${RENDERER_GL_SOURCES} ${RENDERER_VID_INCLUDES} ${RENDERER_VID_SOURCES})
