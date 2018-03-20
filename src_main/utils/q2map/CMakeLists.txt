project(q2map)

# common sources
set(COMMON_INCLUDES
	../common/bspfile.h
	../common/cmdlib.h
	../common/scriplib.h
	../common/lbmlib.h
	../common/mathlib.h
	../common/polylib.h
	../common/threads.h
	)
set(COMMON_SOURCES
	../common/bspfile.c
	../common/cmdlib.c
	../common/scriplib.c
	../common/lbmlib.c
	../common/mathlib.c
	../common/polylib.c
	../common/threads.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})

# main source
set(MAIN_SOURCES
	./main.c
	)

# info source
set(INFO_SOURCES
	./info.c
	)
source_group("info" FILES ${INFO_SOURCES})

# bsp source
set(BSP_INCLUDES
	./qbsp.h
	)
set(BSP_SOURCES
	./brushbsp.c
	./csg.c
	./faces.c
#	./gldraw.c
	./glfile.c
	./leakfile.c
	./map.c
	./nodraw.c
	./portals.c
	./prtfile.c
	./qbsp3.c
	./textures.c
	./tree.c
	./writebsp.c
	)
source_group("bsp" FILES ${BSP_INCLUDES})
source_group("bsp" FILES ${BSP_SOURCES})

# rad source
set(RAD_INCLUDES
	./qrad.h
	)
set(RAD_SOURCES
	./lightmap.c
	./patches.c
	./qrad3.c
	./trace.c
	)
source_group("rad" FILES ${RAD_INCLUDES})
source_group("rad" FILES ${RAD_SOURCES})

# vis sources
set(VIS_INCLUDES
	./vis.h
	)
set(VIS_SOURCES
	./flow.c
	./qvis3.c
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
	)

# build it
add_executable(q2map 
				${MAIN_SOURCES}
				${INFO_SOURCES}
				${BSP_INCLUDES} ${BSP_SOURCES}
				${RAD_INCLUDES} ${RAD_SOURCES}
				${VIS_INCLUDES} ${VIS_SOURCES}
				${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(q2map ${Q_UTILS_DIR})
install(TARGETS q2map RUNTIME DESTINATION ${Q_UTILS_DIR})
