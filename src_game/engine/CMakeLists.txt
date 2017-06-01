project(cake)

#
# Libraries
#

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

# libmad
find_package(MAD)
if (MAD_FOUND)
    include_directories(${MAD_INCLUDE_DIRS})
    set(MAD_LIBRARY ${MAD_LIBRARIES})
else (MAD_FOUND)
    include_directories("../libs/mad/include")
    set(MAD_LIBRARY "" )
endif (MAD_FOUND)

# libmodplug
find_package(MODPLUG)
if (MODPLUG_FOUND)
    include_directories(${MODPLUG_INCLUDE_DIRS})
    set(MODPLUG_LIBRARY ${MODPLUG_LIBRARIES})
else (MODPLUG_FOUND)
    include_directories("../libs/modplug/src")
    set(MODPLUG_LIBRARY "" )
endif (MODPLUG_FOUND)

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

# stb headers
include_directories("../libs/stb")

#
# Source groups
#

# engine side code
set(CLIENT_INCLUDES
	client.h
	cl_bgmusic.h
	console.h
	keys.h
	screen.h
	sound.h
	ref.h
	)
set(CLIENT_SOURCES
	cl_bgmusic.c
	cl_cin.c
	cl_console.c
	cl_ents.c
	cl_fx.c
	cl_input.c
	cl_inv.c
	cl_main.c
	cl_newfx.c
	cl_parse.c
	cl_pred.c
	cl_scrn.c
	cl_tent.c
	cl_view.c
	cl_keys.c
	)
source_group("client" FILES ${CLIENT_INCLUDES})
source_group("client" FILES ${CLIENT_SOURCES})
	
set(MENU_INCLUDES
	qmenu.h
	)	
set(MENU_SOURCES
	qmenu.c
	menu.c
	menu_vid.c
	)
source_group("client\\menu" FILES ${MENU_INCLUDES})
source_group("client\\menu" FILES ${MENU_SOURCES})

set(SOUND_INCLUDES
	snd_codec.h
	snd_codeci.h
	snd_loc.h
	snd_wave.h
	snd_mp3.h
	snd_modplug.h
	)	
set(SOUND_SOURCES
	snd_codec.c
	snd_dma.c
	snd_mem.c
	snd_mix.c
	snd_wave.c
	snd_mp3.c
	snd_modplug.c
	)
source_group("client\\sound" FILES ${SOUND_INCLUDES})
source_group("client\\sound" FILES ${SOUND_SOURCES})

set(COMMON_INCLUDES
	crc.h
	qcommon.h
	)	
set(COMMON_SOURCES
	cmd.c
	common.c
	crc.c
	cvar.c
	files.c
	matrix.c
	md4.c
	net_chan.c
	pmove.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})
	
set(RENDERER_INCLUDES
	gl_model.h
	gl_local.h
	)	
set(RENDERER_SOURCES
	gl_beam.c
	gl_draw.c
	gl_framebuffer.c
	gl_image.c
	gl_light.c
	gl_mesh.c
	gl_model.c
	gl_nullmodel.c
	gl_part.c
	gl_rmain.c
	gl_rmisc.c
	gl_rsurf.c
	gl_sky.c
	gl_sprite.c
	gl_underwater.c
	gl_warp.c
	)
source_group("client\\renderer" FILES ${RENDERER_INCLUDES})
source_group("client\\renderer" FILES ${RENDERER_SOURCES})
	
set(SERVER_INCLUDES
	server.h
	)	
set(SERVER_SOURCES
	sv_ccmds.c
	sv_cmodel.c
	sv_ents.c
	sv_game.c
	sv_init.c
	sv_main.c
	sv_send.c
	sv_user.c
	sv_world.c
	)
source_group("server" FILES ${SERVER_INCLUDES})
source_group("server" FILES ${SERVER_SOURCES})
	
set(SYSTEM_INCLUDES
	q2icon64.h
	vid.h
	vid_common.h
	)
set(SYSTEM_SOURCES
	sys.c
	sys_snd.c
	sys_input.c
	sys_net.c
	sys_vid.c
	sys_glimp.c
	)
source_group("system" FILES ${SYSTEM_INCLUDES})
source_group("system" FILES ${SYSTEM_SOURCES})

# shared code
set(SHARED_INCLUDES
	../shared/game.h
	../shared/q_shared.h
	../shared/qfiles.h
	)	
set(SHARED_SOURCES
	../shared/m_flash.c
	../shared/q_shared.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})

# game dll code
file(GLOB GAME_INCLUDES ../game/*.h)
file(GLOB GAME_SOURCES ../game/*.c)
source_group("game" FILES ${GAME_INCLUDES})
source_group("game" FILES ${GAME_SOURCES})

# windows resources
set(WIN32_RESOURCES
	win32/q2.ico
	win32/q2.rc
	)
source_group("system\\win32\\Resources" FILES ${WIN32_RESOURCES})

# libs

# glew
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

# libmad
if (NOT MAD_FOUND)
	file(GLOB MAD_INCLUDES ../libs/mad/include/*.h)
	file(GLOB MAD_SOURCES ../libs/mad/src/*.c)
else (NOT MAD_FOUND)
    set(MAD_INCLUDES "")
    set(MAD_SOURCES "")
endif (NOT MAD_FOUND)
source_group("libs\\mad" FILES ${MAD_INCLUDES})
source_group("libs\\mad" FILES ${MAD_SOURCES})

# libmodplug
if (NOT MODPLUG_FOUND)
	file(GLOB MODPLUG_INCLUDES ../libs/modplug/src/*.h)
	file(GLOB MODPLUG_SOURCES ../libs/modplug/src/*.cpp)
	add_definitions(-DMODPLUG_STATIC)
else (NOT MODPLUG_FOUND)
    set(MODPLUG_INCLUDES "")
    set(MODPLUG_SOURCES "")
endif (NOT MODPLUG_FOUND)
source_group("libs\\modplug" FILES ${MODPLUG_INCLUDES})
source_group("libs\\modplug" FILES ${MODPLUG_SOURCES})


#
# Engine Executable
#

# main engine executable inlcudes and source list
set(CAKE_INCLUDES
			${CLIENT_INCLUDES}
			${MENU_INCLUDES}
			${SOUND_INCLUDES}
			${COMMON_INCLUDES}
			${RENDERER_INCLUDES}
			${SERVER_INCLUDES}
			${SYSTEM_INCLUDES}
			${SHARED_INCLUDES}

			${GLEW_INCLUDES}
			${MAD_INCLUDES}
			${MODPLUG_INCLUDES}
			)			
set(CAKE_SOURCES
			${CLIENT_SOURCES}
			${MENU_SOURCES}
			${SOUND_SOURCES}
			${COMMON_SOURCES} 
			${RENDERER_SOURCES}
			${SERVER_SOURCES}
			${SYSTEM_SOURCES}
			${SHARED_SOURCES}
			
			${GLEW_SOURCES}
			${MAD_SOURCES}
			${MODPLUG_SOURCES}
			)
	
if(MONOLITH)
	list(APPEND CAKE_INCLUDES
		${GAME_INCLUDES}
		)		
	list(APPEND CAKE_SOURCES
		${GAME_SOURCES}
		)

	add_definitions(-D__MONOLITH_GAME__)
endif()

# set include directories
include_directories(
			.
			../shared
            )

# build it
if(MSVC)

	list(APPEND CAKE_SOURCES ${WIN32_RESOURCES})	
	list(REMOVE_DUPLICATES CAKE_SOURCES)
	
	# build the engine
	add_executable(cake WIN32 ${CAKE_INCLUDES} ${CAKE_SOURCES})
	target_link_libraries(cake PRIVATE
		${SDLx_LIBRARY}
		opengl32
		glu32
		dbghelp
		iphlpapi
		wsock32.lib
        ${GLEW_LIBRARY}
		${CMAKE_DL_LIBS}
		)
	set_target_properties(cake PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
	
else()

	find_package(OpenGL REQUIRED)
	include_directories(${OPENGL_INCLUDE_DIRS})

	list(REMOVE_DUPLICATES CAKE_SOURCES)

	GET_DIRECTORY_PROPERTY(_directory_flags DEFINITIONS)
	LIST(APPEND _compiler_FLAGS ${_directory_flags})
	SEPARATE_ARGUMENTS(_compiler_FLAGS)

	# build the engine
	add_executable(cake WIN32 ${CAKE_SOURCES})
	if(NOT WIN32)
		if(NOT "${CMAKE_SYSTEM}" MATCHES "Darwin")
			set(RT_LIBRARY rt)
		endif()
		target_link_libraries(cake PRIVATE
			${OPENGL_LIBRARIES}
			pthread
			${RT_LIBRARY}
			${SDLx_LIBRARY}
            ${GLEW_LIBRARY}
			${CMAKE_DL_LIBS}
			)
	endif()

endif()

q_set_output_dir(cake "")
install(TARGETS cake RUNTIME DESTINATION .)