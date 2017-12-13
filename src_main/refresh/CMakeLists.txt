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
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
set(SDLx_LIBRARY ${SDL2_LIBRARY})


# directx sdk
find_package(DirectX)
if (DirectX_D3DX9_FOUND)
	include_directories (BEFORE SYSTEM ${DirectX_D3D9_INCLUDE_DIR})
	include_directories (BEFORE SYSTEM ${DirectX_D3DX9_INCLUDE_DIR})
	add_definitions(-DREF_D3D9)
endif()

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
	)
source_group("refresh\\gl" FILES ${RENDERER_GL_INCLUDES})
source_group("refresh\\gl" FILES ${RENDERER_GL_SOURCES})

# D3D9 refresh backend sources
if (DirectX_D3D9_FOUND)
	set(RENDERER_D3D9_INCLUDES
		../refresh/d3d9/d3d_model.h
		../refresh/d3d9/d3d_local.h
		)
	set(RENDERER_D3D9_SOURCES
		../refresh/d3d9/d3d_beam.cpp
		../refresh/d3d9/d3d_draw.cpp
		../refresh/d3d9/d3d_image.cpp
		../refresh/d3d9/d3d_imagefiles.cpp
		../refresh/d3d9/d3d_imagefilters.cpp
		../refresh/d3d9/d3d_imagescrap.cpp
		../refresh/d3d9/d3d_light.cpp
		../refresh/d3d9/d3d_matrix.cpp
		../refresh/d3d9/d3d_mesh.cpp
		../refresh/d3d9/d3d_model.cpp
		../refresh/d3d9/d3d_nullmodel.cpp
		../refresh/d3d9/d3d_part.cpp
		../refresh/d3d9/d3d_rmain.cpp
		../refresh/d3d9/d3d_rmisc.cpp
		../refresh/d3d9/d3d_rsurf.cpp
		../refresh/d3d9/d3d_shader.cpp
		../refresh/d3d9/d3d_sky.cpp
		../refresh/d3d9/d3d_sprite.cpp
		../refresh/d3d9/d3d_state.cpp
		../refresh/d3d9/d3d_warp.cpp
		)
	source_group("refresh\\d3d9" FILES ${RENDERER_D3D9_INCLUDES})
	source_group("refresh\\d3d9" FILES ${RENDERER_D3D9_SOURCES})
else()
	set(RENDERER_D3D9_INCLUDES "")
	set(RENDERER_D3D9_SOURCES "")
endif()

# video backend sources
set(RENDERER_VID_INCLUDES
	../refresh/vid_public.h
	)
set(RENDERER_VID_SOURCES
	../refresh/vid/vid.c
	../refresh/vid/vid_gl.c
	)
if (DirectX_D3D9_FOUND)
	list(APPEND RENDERER_VID_SOURCES ../refresh/vid/vid_d3d9.cpp)
endif()
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
add_library(refresh STATIC ${GLEW_INCLUDES}
	${GLEW_SOURCES}
	${RENDERER_INCLUDES}
	${RENDERER_SOURCES}
	${RENDERER_SOFT_INCLUDES}
	${RENDERER_SOFT_SOURCES}
	${RENDERER_GL_INCLUDES}
	${RENDERER_GL_SOURCES}
	${RENDERER_D3D9_INCLUDES}
	${RENDERER_D3D9_SOURCES}
	${RENDERER_VID_INCLUDES}
	${RENDERER_VID_SOURCES}
	)
