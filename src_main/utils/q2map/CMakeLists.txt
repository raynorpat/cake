project(q2map)

#
# Libraries
#

# sdl2
if(NOT USE_SYSTEM_SDL2)
	SET(SDL2_PATH ../libs/SDL2)
endif()
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

# common sources
set(COMMON_INCLUDES
	../common/bspfile.h
	../common/cmdlib.h
	../common/scriplib.h
	../common/lbmlib.h
	../common/mathlib.h
	../common/polylib.h
	)
set(COMMON_SOURCES
	../common/bspfile.c
	../common/cmdlib.c
	../common/scriplib.c
	../common/lbmlib.c
	../common/mathlib.c
	../common/polylib.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})

# main source
set(MAIN_INCLUDES
	./q2map.h
	./threads.h
	)
set(MAIN_SOURCES
	./main.c
	./console.c
	./threads.c
	)
source_group("main" FILES ${MAIN_INCLUDES})
source_group("main" FILES ${MAIN_SOURCES})

# shared source
set(SHARED_INCLUDES
	../../shared/q_platform.h
	../../shared/q_compiler.h
	../../shared/q_endian.h
	../../shared/q_types.h
	../../shared/q_threads.h
	)
set(SHARED_SOURCES
	../../shared/q_threads.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})

# bsp source
set(BSP_INCLUDES
	./qbsp.h
	)
set(BSP_SOURCES
	./brushbsp.c
	./csg.c
	./faces.c
	./leakfile.c
	./map.c
	./portals.c
	./prtfile.c
	./qbsp.c
	./textures.c
	./tree.c
	./writebsp.c
	)
source_group("bsp" FILES ${BSP_INCLUDES})
source_group("bsp" FILES ${BSP_SOURCES})

# rad source
set(RAD_INCLUDES
	./qlight.h
	)
set(RAD_SOURCES
	./lightmap.c
	./patches.c
	./qlight.c
	./trace.c
	)
source_group("rad" FILES ${RAD_INCLUDES})
source_group("rad" FILES ${RAD_SOURCES})

# vis sources
set(VIS_INCLUDES
	./qvis.h
	)
set(VIS_SOURCES
	./flow.c
	./qvis.c
	)
source_group("vis" FILES ${VIS_INCLUDES})
source_group("vis" FILES ${VIS_SOURCES})


#
# q2map generation tool
#

# set include directories
include_directories(
	.
	../common
	../../shared
	)

# build it
add_executable(q2map 
				${MAIN_INCLUDES} ${MAIN_SOURCES}
				${SHARED_INCLUDES} ${SHARED_SOURCES}
				${BSP_INCLUDES} ${BSP_SOURCES}
				${RAD_INCLUDES} ${RAD_SOURCES}
				${VIS_INCLUDES} ${VIS_SOURCES}
				${COMMON_INCLUDES} ${COMMON_SOURCES})
				
target_link_libraries(q2map ${SDL2_LIBRARY})

q_set_output_dir(q2map ${Q_UTILS_DIR})
install(TARGETS q2map RUNTIME DESTINATION ${Q_UTILS_DIR})
