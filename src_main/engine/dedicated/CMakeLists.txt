project(cake_ded)

#
# Options
#
add_definitions(-DDEDICATED_ONLY)

#
# Libraries
#

# sdl2
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH ../libs/SDL2)
endif()
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

# zlib
find_package(ZLIB)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
    set(ZLIB_LIBRARY ${ZLIB_LIBRARIES})
else (ZLIB_FOUND)
    include_directories("../../libs/zlib")
    set(ZLIB_LIBRARY "" )
	message(STATUS "Using builtin zlib")
endif (ZLIB_FOUND)

# minizip headers
include_directories("../../libs/minizip")

#
# Source groups
#

# engine side code
set(CLIENT_INCLUDES
	../client.h
	)
set(CLIENT_SOURCES
	cl_null.c
	)
source_group("client" FILES ${CLIENT_INCLUDES})
source_group("client" FILES ${CLIENT_SOURCES})

set(COMMON_INCLUDES
	../crc.h
	../qcommon.h
	)
set(COMMON_SOURCES
	../cmd.c
	../common.c
	../crc.c
	../cvar.c
	../files.c
	../md4.c
	../net.c
	../net_chan.c
	../pmove.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})

set(SERVER_INCLUDES
	../server.h
	)
set(SERVER_SOURCES
	../sv_ccmds.c
	../sv_cmodel.c
	../sv_ents.c
	../sv_game.c
	../sv_init.c
	../sv_main.c
	../sv_send.c
	../sv_user.c
	../sv_world.c
	)
source_group("server" FILES ${SERVER_INCLUDES})
source_group("server" FILES ${SERVER_SOURCES})

set(SYSTEM_INCLUDES
	../sdl/q2icon64.h
	)
set(SYSTEM_SOURCES
	../sdl/sys_sdl.c
	../sys_misc.c
	)
source_group("system\\sdl" FILES ${SYSTEM_INCLUDES})
source_group("system\\sdl" FILES ${SYSTEM_SOURCES})

# shared code
set(SHARED_INCLUDES
	../../shared/game.h
	../../shared/q_platform.h
	../../shared/q_shared.h
	../../shared/qfiles.h
	)
set(SHARED_SOURCES
	../../shared/m_flash.c
	../../shared/q_shared.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})

# game dll code
file(GLOB GAME_INCLUDES ../../game/*.h)
file(GLOB GAME_SOURCES ../../game/*.c)
source_group("game" FILES ${GAME_INCLUDES})
source_group("game" FILES ${GAME_SOURCES})

# windows code and resources
set(WIN32_RESOURCES
	../win32/q2.ico
	../win32/q2.rc
	)
set(WIN32_SOURCES
	../win32/con_win32.c
	)
source_group("system\\win32" FILES ${WIN32_SOURCES})
source_group("system\\win32\\Resources" FILES ${WIN32_RESOURCES})

# posix code
set(POSIX_SOURCES
	../sdl/con_tty.c
	)
source_group("system\\posix" FILES ${POSIX_SOURCES})

# zlib
if (NOT ZLIB_FOUND)
	file(GLOB ZLIB_INCLUDES ../../libs/zlib/*.h)
	file(GLOB ZLIB_SOURCES ../../libs/zlib/*.c)
else (NOT ZLIB_FOUND)
    set(ZLIB_INCLUDES "")
    set(ZLIB_SOURCES "")
endif (NOT ZLIB_FOUND)
source_group("libs\\zlib" FILES ${ZLIB_INCLUDES})
source_group("libs\\zlib" FILES ${ZLIB_SOURCES})

# minizip
file(GLOB MINIZIP_INCLUDES ../../libs/minizip/*.h)
file(GLOB MINIZIP_SOURCES ../../libs/minizip/*.c)
source_group("libs\\minizip" FILES ${MINIZIP_INCLUDES})
source_group("libs\\minizip" FILES ${MINIZIP_SOURCES})

#
# Engine Executable
#

# main engine executable includes and source list
set(CAKE_INCLUDES
			${CLIENT_INCLUDES}
			${COMMON_INCLUDES}
			${SERVER_INCLUDES}
			${SYSTEM_INCLUDES}
			${SHARED_INCLUDES}
			
			${ZLIB_INCLUDES}
			${MINIZIP_INCLUDES}
			)
set(CAKE_SOURCES
			${CLIENT_SOURCES}
			${COMMON_SOURCES}
			${SERVER_SOURCES}
			${SYSTEM_SOURCES}
			${SHARED_SOURCES}
			
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

# set include directories
include_directories(
			.
			../
			../../shared
			../../refresh
            )

# build it
if(MSVC)

	list(APPEND CAKE_SOURCES ${WIN32_RESOURCES} ${WIN32_SOURCES})
	list(REMOVE_DUPLICATES CAKE_SOURCES)

	# build the engine
	add_executable(cake_ded ${CAKE_INCLUDES} ${CAKE_SOURCES})
	target_link_libraries(cake_ded PRIVATE
		${SDL2_LIBRARY}
		${ZLIB_LIBRARY}
		dbghelp
		iphlpapi
		wsock32.lib
		${CMAKE_DL_LIBS}
		)
	set_target_properties(cake_ded PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
	
else()

	list(APPEND CAKE_SOURCES ${POSIX_SOURCES})
	list(REMOVE_DUPLICATES CAKE_SOURCES)

	GET_DIRECTORY_PROPERTY(_directory_flags DEFINITIONS)
	LIST(APPEND _compiler_FLAGS ${_directory_flags})
	SEPARATE_ARGUMENTS(_compiler_FLAGS)

	# build the engine
	add_executable(cake_ded ${CAKE_SOURCES})
	if(NOT WIN32)
		if(NOT "${CMAKE_SYSTEM}" MATCHES "Darwin")
			set(RT_LIBRARY rt)
		endif()
		target_link_libraries(cake_ded PRIVATE
			pthread
			${RT_LIBRARY}
			${SDL2_LIBRARY}
			${ZLIB_LIBRARY}
			${CMAKE_DL_LIBS}
			)
	endif()

endif()

q_set_output_dir(cake_ded "")
install(TARGETS cake_ded RUNTIME DESTINATION .)
