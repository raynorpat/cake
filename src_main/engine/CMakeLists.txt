# add dedicated server project
if (NOT(WIN_UWP))
	add_subdirectory(dedicated)
endif()

# base engine
project(cake)

#
# Options
#
if (USE_CODEC_OGG)
	add_definitions(-DUSE_CODEC_OGG)
endif()
if (USE_OPENAL)
	add_definitions(-DUSE_OPENAL)
endif()

#
# Libraries
#

# sdl2
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH ../libs/SDL2)
endif()
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

# openal
if (USE_OPENAL)
	if(NOT USE_SYSTEM_OPENAL)
		set(ENV{OPENALDIR} ${CMAKE_SOURCE_DIR}/libs/openal)
	endif()
	find_package(OPENAL REQUIRED)
	include_directories(${OPENAL_INCLUDE_DIR})
endif()

# libogg & libvorbis
if (USE_CODEC_OGG)
	find_package(OGG)
	if (OGG_FOUND)
		include_directories(${OGG_INCLUDE_DIRS})
		set(OGG_LIBRARY ${OGG_LIBRARIES})
	else (OGG_FOUND)
		include_directories("../libs/libogg/include")
		set(OGG_LIBRARY "")
		message(STATUS "Using builtin libogg")
	endif (OGG_FOUND)
	
	find_package(VORBIS)
	if (VORBIS_FOUND)
		include_directories(${VORBIS_INCLUDE_DIRS})
		set(VORBIS_LIBRARY ${VORBIS_LIBRARIES})
	else (VORBIS_FOUND)
		include_directories("../libs/libvorbis/include")
		set(VORBIS_LIBRARY "")
		message(STATUS "Using builtin libvorbis")
	endif (VORBIS_FOUND)
endif()	

# glew
find_package(GLEW)
if (GLEW_FOUND)
    include_directories(${GLEW_INCLUDE_DIRS})
    set(GLEW_LIBRARY ${GLEW_LIBRARIES})
else (GLEW_FOUND)
    include_directories("../libs/glew/include")
    set(GLEW_LIBRARY "")
    add_definitions(-DGLEW_STATIC)
	message(STATUS "Using builtin glew")
endif (GLEW_FOUND)

# libcurl
find_package(CURL)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    set(CURL_LIBRARY ${CURL_LIBRARIES})
else (CURL_FOUND)
    include_directories("../libs/libcurl/include")
	include_directories("../libs/libcurl/lib")
    set(CURL_LIBRARY "" )
	message(STATUS "Using builtin libcurl")
endif (CURL_FOUND)

# zlib
find_package(ZLIB)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
    set(ZLIB_LIBRARY ${ZLIB_LIBRARIES})
else (ZLIB_FOUND)
    include_directories("../libs/zlib")
    set(ZLIB_LIBRARY "" )
	message(STATUS "Using builtin zlib")
endif (ZLIB_FOUND)

# gamespy library
include_directories("../libs/gamespy")

# stb headers
include_directories("../libs/stb")

# minizip headers
include_directories("../libs/minizip")


#
# Source groups
#

# engine side code
set(CLIENT_INCLUDES
	client.h
	console.h
	keys.h
	screen.h
	sound.h
	)
set(CLIENT_SOURCES
	cl_cin.c
	cl_console.c
	cl_ents.c
	cl_fx.c
	cl_gamespy.c
	cl_http.c
	cl_input.c
	cl_inv.c
	cl_main.c
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
	menu_main.c
	menu_multiplayer.c
	menu_singleplayer.c
	menu_options.c
	menu_vid.c
	)
source_group("client\\menu" FILES ${MENU_INCLUDES})
source_group("client\\menu" FILES ${MENU_SOURCES})

set(SOUND_INCLUDES
	snd_local.h
	snd_wave.h
	)
set(SOUND_SOURCES
	snd_dma.c
	snd_mem.c
	snd_mix.c
	snd_wave.c
	)
if (USE_OPENAL)
	list(APPEND SOUND_INCLUDES snd_qal.h)
	list(APPEND SOUND_SOURCES snd_qal.c snd_al.c)
endif()
if (USE_CODEC_OGG)
	list(APPEND SOUND_INCLUDES snd_ogg.h)
	list(APPEND SOUND_SOURCES snd_ogg.c)
endif()
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
	md4.c
	net.c
	net_chan.c
	pmove.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})

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
	sdl/q2icon64.h
	)
set(SYSTEM_SOURCES
	sdl/sys_sdl.c
	sdl/sdl_snd.c
	sdl/sdl_input.c
	sys_misc.c
	)
source_group("system\\sdl" FILES ${SYSTEM_INCLUDES})
source_group("system\\sdl" FILES ${SYSTEM_SOURCES})

# shared code
set(SHARED_INCLUDES
	../shared/game.h
	../shared/q_platform.h
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
	file(GLOB GLEW_INCLUDES ../libs/glew/include/GL/*.h)
	file(GLOB GLEW_SOURCES ../libs/glew/src/*.c)
else (NOT GLEW_FOUND)
    set(GLEW_INCLUDES "")
    set(GLEW_SOURCES "")
endif (NOT GLEW_FOUND)
source_group("libs\\glew" FILES ${GLEW_INCLUDES})
source_group("libs\\glew" FILES ${GLEW_SOURCES})

# libcurl
if (NOT CURL_FOUND)
	file(GLOB CURL_INCLUDES ../libs/libcurl/include/curl/*.h)
	file(GLOB CURL_SOURCES ../libs/libcurl/lib/*.c ../libs/libcurl/lib/vauth/*.c ../libs/libcurl/lib/vtls/*.c)
	add_definitions(-DCURL_STATICLIB
					-DBUILDING_LIBCURL 
					-DCURL_DISABLE_LDAP -DCURL_DISABLE_CRYPTO_AUTH)
else (NOT CURL_FOUND)
    set(CURL_INCLUDES "")
    set(CURL_SOURCES "")
endif (NOT CURL_FOUND)
source_group("libs\\curl" FILES ${CURL_INCLUDES})
source_group("libs\\curl" FILES ${CURL_SOURCES})

# libogg and libvorbis
if (USE_CODEC_OGG)
	if (NOT OGG_FOUND)
		file(GLOB OGG_INCLUDES ../libs/libogg/include/ogg/*.h)
		file(GLOB OGG_SOURCES ../libs/libogg/src/*.c)
	else (NOT OGG_FOUND)
		set(OGG_INCLUDES "")
		set(OGG_SOURCES "")
	endif (NOT OGG_FOUND)
	source_group("libs\\libogg" FILES ${OGG_INCLUDES})
	source_group("libs\\libogg" FILES ${OGG_SOURCES})
	
	if (NOT VORBIS_FOUND)
		file(GLOB VORBIS_INCLUDES ../libs/libvorbis/include/vorbis/*.h
									../libs/libvorbis/lib/modes/*.h
									../libs/libvorbis/lib/books/uncoupled/*.h
									../libs/libvorbis/lib/books/floor/*.h
									../libs/libvorbis/lib/books/coupled/*.h)
		file(GLOB VORBIS_SOURCES ../libs/libvorbis/lib/*.c)
	else (NOT VORBIS_FOUND)
		set(VORBIS_INCLUDES "")
		set(VORBIS_SOURCES "")
	endif (NOT VORBIS_FOUND)
	source_group("libs\\libvorbis" FILES ${VORBIS_INCLUDES})
	source_group("libs\\libvorbis" FILES ${VORBIS_SOURCES})	
endif()

# zlib
if (NOT ZLIB_FOUND)
	file(GLOB ZLIB_INCLUDES ../libs/zlib/*.h)
	file(GLOB ZLIB_SOURCES ../libs/zlib/*.c)
else (NOT ZLIB_FOUND)
    set(ZLIB_INCLUDES "")
    set(ZLIB_SOURCES "")
endif (NOT ZLIB_FOUND)
source_group("libs\\zlib" FILES ${ZLIB_INCLUDES})
source_group("libs\\zlib" FILES ${ZLIB_SOURCES})

# minizip
file(GLOB MINIZIP_INCLUDES ../libs/minizip/*.h)
file(GLOB MINIZIP_SOURCES ../libs/minizip/*.c)
source_group("libs\\minizip" FILES ${MINIZIP_INCLUDES})
source_group("libs\\minizip" FILES ${MINIZIP_SOURCES})

# gamespy library
file(GLOB GAMESPY_INCLUDES ../libs/gamespy/*.h ../libs/gamespy/CEngine/*.h)
file(GLOB GAMESPY_SOURCES ../libs/gamespy/*.c ../libs/gamespy/CEngine/*.c)
source_group("libs\\gamespy" FILES ${GAMESPY_INCLUDES})
source_group("libs\\gamespy" FILES ${GAMESPY_SOURCES})


#
# Engine Executable
#

# main engine executable includes and source list
set(CAKE_INCLUDES
			${CLIENT_INCLUDES}
			${MENU_INCLUDES}
			${SOUND_INCLUDES}
			${COMMON_INCLUDES}
			${SERVER_INCLUDES}
			${SYSTEM_INCLUDES}
			${SHARED_INCLUDES}

			${GAMESPY_INCLUDES}
			${CURL_INCLUDES}
			${OGG_INCLUDES}
			${VORBIS_INCLUDES}
			${ZLIB_INCLUDES}
			${MINIZIP_INCLUDES}
			)
set(CAKE_SOURCES
			${CLIENT_SOURCES}
			${MENU_SOURCES}
			${SOUND_SOURCES}
			${COMMON_SOURCES}
			${SERVER_SOURCES}
			${SYSTEM_SOURCES}
			${SHARED_SOURCES}

			${GAMESPY_SOURCES}
			${CURL_SOURCES}
			${OGG_SOURCES}
			${VORBIS_SOURCES}
			${ZLIB_SOURCES}
			${MINIZIP_SOURCES}
			)

if (MONOLITH)
	list(APPEND CAKE_INCLUDES
		${GAME_INCLUDES}
		)
	list(APPEND CAKE_SOURCES
		${GAME_SOURCES}
		)

	add_definitions(-DMONOLITH)
endif()

if (WIN_UWP)
	# set manifest names and guid
	set(APP_MANIFEST_NAME Package.appxmanifest)
	set(EXE_NAME cake)
	set(SHORT_NAME ${EXE_NAME})
	set(PACKAGE_GUID "6515377e-dfd4-4cdb-80df-4e0326346efc")

	# setup application manifest
	configure_file(
		winrt/${APP_MANIFEST_NAME}
		${CMAKE_CURRENT_BINARY_DIR}/${APP_MANIFEST_NAME}
		@ONLY
		)

	# sources
	set(WIN_UWP_SOURCES
		winrt/SDL_winrt_main_NonXAML.cpp
		)
	source_group("system\\winrt" FILES ${WIN_UWP_SOURCES})
	
	# resources
	set(WIN_UWP_RESOURCES
		${CMAKE_CURRENT_BINARY_DIR}/${APP_MANIFEST_NAME}
		winrt/TemporaryKey.pfx
		winrt/SDL2-WinRTResource_BlankCursor.cur
		winrt/SDL2-WinRTResources.rc
		)
	source_group("system\\winrt\\Resources" FILES ${WIN_UWP_RESOURCES})
	set(ASSET_FILES
		winrt/assets/LockScreenLogo.scale-200.png
		winrt/assets/SplashScreen.scale-200.png
		winrt/assets/Square44x44Logo.scale-200.png
		winrt/assets/Square44x44Logo.targetsize-24_altform-unplated.png
		winrt/assets/Square150x150Logo.scale-200.png
		winrt/assets/StoreLogo.png
		winrt/assets/Wide310x150Logo.scale-200.png
	)
	source_group("system\\winrt\\Assets" FILES ${ASSET_FILES})
	set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_LOCATION "Assets")

	# append source and resources
	list(APPEND CAKE_SOURCES ${WIN_UWP_RESOURCES} ${WIN_UWP_SOURCES})
endif()

# set include directories
include_directories(
			.
			../shared
			../refresh
            )

# build it
if(MSVC)

	list(APPEND CAKE_SOURCES ${WIN32_RESOURCES})
	list(REMOVE_DUPLICATES CAKE_SOURCES)

	# build the engine
	add_executable(cake WIN32 ${CAKE_INCLUDES} ${CAKE_SOURCES})
if (NOT WIN_UWP)
	target_link_libraries(cake PRIVATE
		${SDL2_LIBRARY}
		opengl32
		glu32
		dbghelp
		iphlpapi
		wsock32.lib
		${CURL_LIBRARY}
        ${GLEW_LIBRARY}
		${ZLIB_LIBRARY}
		${OGG_LIBRARY}
		${VORBIS_LIBRARY}
		${CMAKE_DL_LIBS}
		${OPENAL_LIBRARY}
		refresh
		)
else()
	target_link_libraries(cake PRIVATE
		${SDL2_LIBRARY}
		${ZLIB_LIBRARY}
		${CURL_LIBRARY}
		${OGG_LIBRARY}
		${VORBIS_LIBRARY}
		${CMAKE_DL_LIBS}
		refresh
		)
	target_include_directories(cake PRIVATE .
		../shared
		../refresh
		)
	set_property(TARGET cake PROPERTY VS_WINRT_COMPONENT TRUE)			
endif()		
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
			refresh
			pthread
			${RT_LIBRARY}
			${SDL2_LIBRARY}
            ${GLEW_LIBRARY}
			${CURL_LIBRARY}
			${OGG_LIBRARY}
			${VORBIS_LIBRARY}
			${ZLIB_LIBRARY}
			${OPENGL_LIBRARY}
			${OPENAL_LIBRARY}
			${CMAKE_DL_LIBS}
			)
	endif()

endif()

q_set_output_dir(cake "")
install(TARGETS cake RUNTIME DESTINATION .)
