cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(Cake)

#
# Options
#
		
option(MONOLITH
		"Embed game logic into main executable" OFF)

option(SDL2
		"Use SDL2 instead of SDL1.2" ON)
		
option(ONATIVE
		"Optimize for the host CPU" OFF)
		
option(WINRT
		"Build for Windows RT" OFF)
		
option(WINDOWS10
		"Build for Windows 10" OFF)
		
option(USE_SYSTEM_LIBGLEW
                "Use the system libglew instead of the bundled one" OFF)
				
option(USE_SYSTEM_SDL2
                "Use the system SDL2 instead of the bundled one" OFF)

set(CPU_OPTIMIZATION "-mmmx -msse -msse2" CACHE STRING "Which CPU specific optimitations should be used beside the compiler's default?")

option(USE_INTRINSICS "Compile using intrinsics (e.g mmx, sse, msse2)" ON)

if(MSVC)
	#message(STATUS CMAKE_ROOT: ${CMAKE_ROOT})
	
	#if(CMAKE_CL_64)
	#	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../bin/win64)
	#else()
	#	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../bin/win32)
	#endif()
	
	#message(STATUS EXECUTABLE_OUTPUT_PATH: ${EXECUTABLE_OUTPUT_PATH})
	#message(STATUS PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR})
	#message(STATUS CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR})
else()
	message(STATUS CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE})
endif()

#
# Compiler settings
#

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	add_definitions(-pipe)
	#add_definitions(-Wall)
	add_definitions(-Werror=format-security)
	add_definitions(-Werror=format)
	if (CPU_OPTIMIZATION)
		add_definitions(${CPU_OPTIMIZATION})
	endif()
	if (USE_INTRINSICS)
		add_definitions(-DUSE_INTRINSICS)
	endif()	
	#if(NOT ANDROID)
		add_definitions(-DUSE_EXCEPTIONS)
	#endif()
	
	# the warnings are used for every profile anyway, so put them in a variable
	set(my_warn_flags "-Wno-pragmas -Wno-unused-variable -Wno-switch -Wno-unused-value -Winvalid-pch -Wno-multichar")
	
	if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
		# append clang-specific settings for warnings (the second one make sure clang doesn't complain
		# about unknown -W flags, like -Wno-unused-but-set-variable)
		set(my_warn_flags "${my_warn_flags} -Wno-local-type-template-args -Wno-unknown-warning-option -Wno-inline-new-delete -Wno-switch-enum")
	endif()
	
	if(NOT CMAKE_CROSSCOMPILING AND ONATIVE)
		add_definitions(-march=native)
	endif()

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG -O0 -ggdb -fno-strict-aliasing ${my_warn_flags}")
	#set(CMAKE_C_FLAGS_DEBUGALL "${CMAKE_C_FLAGS_DEBUGALL} -g -ggdb -D_DEBUG -fno-strict-aliasing ${my_warn_flags}")
	#set(CMAKE_C_FLAGS_PROFILE "${CMAKE_C_FLAGS_PROFILE} -g -ggdb -D_DEBUG -O1 -fno-omit-frame-pointer -fno-strict-aliasing ${my_warn_flags}")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -ffast-math -fno-unsafe-math-optimizations -fomit-frame-pointer -fno-strict-aliasing ${my_warn_flags}")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -g -O3 -ffast-math -fno-unsafe-math-optimizations -fomit-frame-pointer -fno-strict-aliasing ${my_warn_flags}")
	set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -Os -ffast-math -fno-unsafe-math-optimizations -fomit-frame-pointer -fno-strict-aliasing ${my_warn_flags}")
	
	set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
	#set(CMAKE_CXX_FLAGS_DEBUGALL ${CMAKE_C_FLAGS_DEBUGALL})
	#set(CMAKE_CXX_FLAGS_PROFILE ${CMAKE_C_FLAGS_PROFILE})
	set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})
	set(CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})

elseif(MSVC)	
	# Multiprocessor support for compiling
	# /MP
	
	# /MT  use multithreaded, static runtime library
	#      (=> binary won't need MSVCP100D.dll or similar)
	# /MTd the same with debug symbols
	# /MD and /MDd - the same with dynamic runtime lib
	#     (=> MSVCP100D.dll or similar is needed to run binary)

	# Inline Function Expansion: Any Suitable (/Ob2)
	# "/Ob2",
			
	# enable Intrinsic Functions
	# "/Oi",
			
	# Omit Frame Pointers
	# "/Oy",
	
	# if(MSVC_VERSION EQUAL 1700)
		# #message(STATUS "$ENV{LIBPATH}")
		
		# find_path(Windows_winmd_DIR NAMES Windows.winmd
			# HINTS
			# C:/Program Files (x86)/Windows Kits/8.0/References/CommonConfiguration/Neutral
			# PATH_SUFFIXES "Neutral")
			
		# message(STATUS "${Windows_winmd_DIR}")
		
		# #set(WINRT_OPTIONS "/ZW /D USE_WINRT /AI \"C:\Program Files (x86)\Windows Kits\8.0\References\CommonConfiguration\Neutral\"")
		# #set(WINRT_OPTIONS "/ZW /D USE_WINRT /AI \"$ENV{LIBPATH}\" ")
		# set(WINRT_OPTIONS "/ZW /D USE_WINRT")
	
		# set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D _DEBUG /MP ${WINRT_OPTIONS}")
		# set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MP /Oi /Oy ${WINRT_OPTIONS}")
		# set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MP /Oi ${WINRT_OPTIONS}")
		# set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MP /Oi /Oy ${WINRT_OPTIONS}")
		
		# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MP ${WINRT_OPTIONS}")
		# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MP /Oi /Oy ${WINRT_OPTIONS}")
		# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MP ${WINRT_OPTIONS}")
		# set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MP /Oi /Oy ${WINRT_OPTIONS}")
	# else()
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D _DEBUG /MP /MTd")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MP /Oi /Oy /MT")
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MP /Oi /MTd")
		set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MP /Oi /Oy /MT")
		
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MP /MTd")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MP /Oi /Oy /MT")
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MP /MTd")
		set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MP /Oi /Oy /MT")
		
		# RB: without /SAFESEH:NO we can't link against ffmpeg libs and VS2013 or we have to build our own
		# libs for newer compilers
		set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
		set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /SAFESEH:NO")
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SAFESEH:NO")
		set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /SAFESEH:NO")
	# endif()	

	add_definitions(-DWIN32 
					-D_WINDOWS
					-D_CRT_SECURE_NO_DEPRECATE
					-D_CRT_NONSTDC_NO_DEPRECATE
					-D_CRT_SECURE_NO_WARNINGS
					-DUSE_EXCEPTIONS)
        ## Check for Version ##
        if( WINRT OR WINDOWS10 ) # Windows RT
			add_definitions(-DUSE_WINRT)
        endif()
        if (USE_INTRINSICS)
			add_definitions(-DUSE_INTRINSICS)
        endif()
endif()

#
# Libraries
#

if (USE_SYSTEM_LIBGLEW)
find_package(GLEW)
endif(USE_SYSTEM_LIBGLEW)

if (GLEW_FOUND)
    include_directories(${GLEW_INCLUDE_DIRS})
    set(GLEW_LIBRARY ${GLEW_LIBRARIES})
else (GLEW_FOUND)
    include_directories("libs/glew/include")
    set(GLEW_LIBRARY "" )
    add_definitions(-DGLEW_STATIC)
endif (GLEW_FOUND)



#
# Source groups
#

# engine side code
set(CLIENT_INCLUDES
	engine/client.h
	engine/console.h
	engine/keys.h
	engine/screen.h
	engine/ref.h
	)
set(CLIENT_SOURCES
	engine/cl_cin.c
	engine/cl_console.c
	engine/cl_ents.c
	engine/cl_fx.c
	engine/cl_input.c
	engine/cl_inv.c
	engine/cl_main.c
	engine/cl_newfx.c
	engine/cl_parse.c
	engine/cl_pred.c
	engine/cl_scrn.c
	engine/cl_tent.c
	engine/cl_view.c
	engine/keys.c
	)
source_group("client" FILES ${CLIENT_INCLUDES})
source_group("client" FILES ${CLIENT_SOURCES})
	
set(MENU_INCLUDES
	engine/qmenu.h
	)	
set(MENU_SOURCES
	engine/qmenu.c
	engine/menu.c
	engine/menu_vid.c
	)
source_group("client\\menu" FILES ${MENU_INCLUDES})
source_group("client\\menu" FILES ${MENU_SOURCES})

set(SOUND_INCLUDES
	engine/snd_loc.h
	)	
set(SOUND_SOURCES
	engine/snd_dma.c
	engine/snd_mem.c
	engine/snd_mix.c
	)
source_group("client\\sound" FILES ${SOUND_INCLUDES})
source_group("client\\sound" FILES ${SOUND_SOURCES})

set(COMMON_INCLUDES
	engine/crc.h
	engine/qcommon.h
	)	
set(COMMON_SOURCES
	engine/cmd.c
	engine/common.c
	engine/crc.c
	engine/cvar.c
	engine/files.c
	engine/matrix.c
	engine/md4.c
	engine/net_chan.c
	engine/pmove.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})
	
set(RENDERER_INCLUDES
	engine/gl_model.h
	engine/gl_local.h
	)	
set(RENDERER_SOURCES
	engine/gl_beam.c
	engine/gl_draw.c
	engine/gl_image.c
	engine/gl_light.c
	engine/gl_mesh.c
	engine/gl_model.c
	engine/gl_nullmodel.c
	engine/gl_part.c
	engine/gl_rmain.c
	engine/gl_rmisc.c
	engine/gl_rsurf.c
	engine/gl_sky.c
	engine/gl_sprite.c
	engine/gl_underwater.c
	engine/gl_warp.c
	)
source_group("client\\renderer" FILES ${RENDERER_INCLUDES})
source_group("client\\renderer" FILES ${RENDERER_SOURCES})
	
set(SERVER_INCLUDES
	engine/server.h
	)	
set(SERVER_SOURCES
	engine/sv_ccmds.c
	engine/sv_cmodel.c
	engine/sv_ents.c
	engine/sv_game.c
	engine/sv_init.c
	engine/sv_main.c
	engine/sv_send.c
	engine/sv_user.c
	engine/sv_world.c
	)
source_group("server" FILES ${SERVER_INCLUDES})
source_group("server" FILES ${SERVER_SOURCES})
	
set(SYSTEM_INCLUDES
	engine/resource.h
	engine/q2icon64.h
	engine/vid.h
	engine/vid_common.h
	engine/sound.h
	engine/cdaudio.h
	)	
set(SYSTEM_SOURCES
	engine/sys.c
	engine/cd_sdl.c
	engine/snd_sdl.c
	engine/input.c
	engine/net.c
	engine/vid.c
	engine/gl_imp.c
	)
source_group("system" FILES ${SYSTEM_INCLUDES})
source_group("system" FILES ${SYSTEM_SOURCES})

# shared code
set(SHARED_INCLUDES
	shared/game.h
	shared/q_shared.h
	shared/qfiles.h
	)	
set(SHARED_SOURCES
	shared/m_flash.c
	shared/q_shared.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})

# game dll code
file(GLOB GAME_INCLUDES game/*.h)
file(GLOB GAME_SOURCES game/*.c)
source_group("game" FILES ${GAME_INCLUDES})
source_group("game" FILES ${GAME_SOURCES})

# windows resources
set(WIN32_RESOURCES
	engine/q2.ico
	engine/q2.rc
	)
source_group("system\\win32\\Resources" FILES ${WIN32_RESOURCES})

#libs
if (NOT GLEW_FOUND)
    set(GLEW_INCLUDES libs/glew/include/GL/glew.h)
    set(GLEW_SOURCES libs/glew/src/glew.c)

    if(WIN32)
        set(GLEW_INCLUDES ${GLEW_INCLUDES} libs/glew/include/GL/wglew.h)
    else(WIN32)
        set(GLEW_INCLUDES ${GLEW_INCLUDES} libs/glew/include/GL/glxew.h)
    endif(WIN32)
else (NOT GLEW_FOUND)
    set(GLEW_INCLUDES "")
    set(GLEW_SOURCES "")
endif (NOT GLEW_FOUND)
source_group("libs\\glew" FILES ${GLEW_INCLUDES})
source_group("libs\\glew" FILES ${GLEW_SOURCES})


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
			shared
            )

# set linking options
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH
		libs/SDL2
	)
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
	
if(MSVC)

	list(APPEND CAKE_SOURCES ${WIN32_RESOURCES})	
	list(REMOVE_DUPLICATES CAKE_SOURCES)
	
	# build the engine
	add_executable(Cake WIN32 ${CAKE_INCLUDES} ${CAKE_SOURCES})
	target_link_libraries(Cake
		${SDLx_LIBRARY}
		opengl32
		glu32
		dbghelp
		iphlpapi
		wsock32.lib
        ${GLEW_LIBRARY}
		${CMAKE_DL_LIBS}
		)
	set_target_properties(Cake PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
	)
	
	# build the game dll
	add_library(game MODULE ${SHARED_INCLUDES} ${SHARED_SOURCES} ${GAME_INCLUDES} ${GAME_SOURCES})
	set_target_properties(game PROPERTIES
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release/baseq2
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release/baseq2
	)
	
else()

	find_package(OpenGL REQUIRED)
	include_directories(${OPENGL_INCLUDE_DIRS})

	list(REMOVE_DUPLICATES CAKE_SOURCES)

	GET_DIRECTORY_PROPERTY(_directory_flags DEFINITIONS)
	LIST(APPEND _compiler_FLAGS ${_directory_flags})
	SEPARATE_ARGUMENTS(_compiler_FLAGS)

	# build the engine
	add_executable(Cake WIN32 ${CAKE_SOURCES})
	if(NOT WIN32)
		if(NOT "${CMAKE_SYSTEM}" MATCHES "Darwin")
			set(RT_LIBRARY rt)
		endif()
		target_link_libraries(Cake
			${OPENGL_LIBRARIES}
			pthread
			${RT_LIBRARY}
			${SDLx_LIBRARY}
            ${GLEW_LIBRARY}
			${CMAKE_DL_LIBS}
			)
	endif()
	set_target_properties(Cake PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
	)	
	
	# build the game dll
	add_library(game MODULE ${SHARED_INCLUDES} ${SHARED_SOURCES} ${GAME_INCLUDES} ${GAME_SOURCES})
	set_target_properties(game PROPERTIES
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release/baseq2
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release/baseq2
	)
endif()


