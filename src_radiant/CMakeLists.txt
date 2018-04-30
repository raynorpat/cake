cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(GTKRadiant C CXX)

option(BUILD_RADIANT "Build the gui" ON)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "../../radiant" CACHE PATH "..." FORCE)
endif ()

# output some status messages
message(STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM})
message(STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
message(STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION})
message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})

#-----------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------

set(GTKRadiant_VERSION_MAJOR 1)
set(GTKRadiant_VERSION_MINOR 5)
set(GTKRadiant_VERSION_PATCH 0)
set(GTKRadiant_VERSION "${GTKRadiant_VERSION_MAJOR}.${GTKRadiant_VERSION_MINOR}.${GTKRadiant_VERSION_PATCH}")

file(WRITE "${PROJECT_BINARY_DIR}/RADIANT_MAJOR" ${GTKRadiant_VERSION_MAJOR})
file(WRITE "${PROJECT_BINARY_DIR}/RADIANT_MINOR" ${GTKRadiant_VERSION_MINOR})

set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")

find_package(Git REQUIRED)
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(RADIANT_VERSION_STRING "${GTKRadiant_VERSION}")
if (GIT_VERSION)
    set(RADIANT_VERSION_STRING "${RADIANT_VERSION_STRING}-git-${GIT_VERSION}")
endif ()

message(STATUS "Building ${PROJECT_NAME} ${RADIANT_VERSION_STRING} ${RADIANT_ABOUTMSG}")

#-----------------------------------------------------------------------
# Language standard
#-----------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_VERSION VERSION_LESS "3.1")
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
		include(CheckCXXCompilerFlag)
		check_cxx_compiler_flag(--std=c++${CMAKE_CXX_STANDARD} STD_CXX)
		if (STD_CXX)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++${CMAKE_CXX_STANDARD}")
		else ()
			message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
		endif ()
	else ()
		message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
	endif ()
endif ()

#-----------------------------------------------------------------------
# Flags
#-----------------------------------------------------------------------

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
	macro(addflags args)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${args}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${args}")
	endmacro()
	addflags("-fno-strict-aliasing")
	if (NOT WIN32)
		addflags("-fvisibility=hidden")
	endif ()
elseif(MSVC)
	# define the standard link libraries
	set(CMAKE_CXX_STANDARD_LIBRARIES "kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib dbghelp.lib wsock32.lib ws2_32.lib rpcrt4.lib wininet.lib")
	set(CMAKE_C_STANDARD_LIBRARIES "kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib dbghelp.lib wsock32.lib ws2_32.lib rpcrt4.lib wininet.lib")
		
	# for MSVC 2013+, without /SAFESEH:NO, we can't link against older libraries
	if (NOT(MSVC_VERSION LESS 1800))
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")		
	endif()

	add_definitions(-DWIN32
					-D_WINDOWS
					-D_CRT_SECURE_NO_DEPRECATE
					-D_CRT_NONSTDC_NO_DEPRECATE
					-D_CRT_SECURE_NO_WARNINGS)

	set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "radiant")
	set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE 1)

set(GTK_TARGET 2 CACHE STRING "GTK target")
add_definitions(-DGTK_TARGET=${GTK_TARGET})

#-----------------------------------------------------------------------
# Defs
#-----------------------------------------------------------------------

add_definitions(-DRADIANT_VERSION="${GTKRadiant_VERSION}")
add_definitions(-DRADIANT_MAJOR_VERSION="${GTKRadiant_VERSION_MAJOR}")
add_definitions(-DRADIANT_MINOR_VERSION="${GTKRadiant_VERSION_MINOR}")

add_definitions(-DRADIANT_ABOUTMSG="${GTKRadiant_ABOUT}")

if (NOT CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-D_DEBUG=1)
endif ()

if (APPLE)
    option(XWINDOWS "Build against X11" ON)
    add_definitions(
            -DPOSIX=1
    )
elseif (WIN32)
    add_definitions(
            -DWIN32=1
            -D_WIN32=1
    )
else ()
    set(XWINDOWS ON)
    add_definitions(
            -DPOSIX=1
    )
endif ()

if (XWINDOWS)
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
    add_definitions(-DXWINDOWS=1)
endif ()

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/libs")

#-----------------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------------

add_subdirectory(libs)
add_subdirectory(include)

#-----------------------------------------------------------------------
# Plugins
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(contrib)
endif ()

#-----------------------------------------------------------------------
# Modules
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(plugins)
endif ()

#-----------------------------------------------------------------------
# Radiant
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(radiant build)
endif ()

if (CMAKE_EXECUTABLE_SUFFIX)
    string(REGEX REPLACE "^[.]" "" RADIANT_EXECUTABLE ${CMAKE_EXECUTABLE_SUFFIX})
else ()
    execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE RADIANT_EXECUTABLE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

if (BUILD_RADIANT)
    set_target_properties(radiant PROPERTIES
            COMPILE_DEFINITIONS RADIANT_EXECUTABLE="${RADIANT_EXECUTABLE}"
            )
endif ()

#-----------------------------------------------------------------------
# Game packs
#-----------------------------------------------------------------------

option(DOWNLOAD_GAMEPACKS "Download game packs" OFF)
add_custom_target(game_packs
        COMMAND ${CMAKE_COMMAND} -E make_directory games
        COMMAND DOWNLOAD_GAMEPACKS=yes SOURCE_DIR="${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/install-gamepacks.sh" "${PROJECT_BINARY_DIR}"
        COMMENT "Downloading game packs"
        )
if (DOWNLOAD_GAMEPACKS)
    add_custom_target(game_packs_all ALL DEPENDS game_packs)
endif ()

#-----------------------------------------------------------------------
# Install
#-----------------------------------------------------------------------

install(
        FILES
        "${PROJECT_BINARY_DIR}/RADIANT_MAJOR"
        "${PROJECT_BINARY_DIR}/RADIANT_MINOR"
        DESTINATION ${CMAKE_INSTALL_PREFIX}
)
file(GLOB GAME_FILES "${PROJECT_SOURCE_DIR}/games/*")
unset(_tmp)
foreach (it ${GAME_FILES})
list(APPEND _tmp "${it}/games")
    file(GLOB GAME_DIR "${it}/*.game")
    list(APPEND _tmp "${GAME_DIR}")
endforeach ()
set(GAME_FILES ${_tmp})

install(
        DIRECTORY
        ${GAME_FILES}
        DESTINATION ${CMAKE_INSTALL_PREFIX}
)
