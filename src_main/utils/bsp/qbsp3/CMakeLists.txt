project(qbsp3)

# sources
set(QBSP_INCLUDES
	./qbsp.h
	)
set(QBSP_SOURCES
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
source_group("qbsp3" FILES ${QBSP_INCLUDES})
source_group("qbsp3" FILES ${QBSP_SOURCES})

set(COMMON_INCLUDES
	../../common/bspfile.h
	../../common/cmdlib.h
	../../common/scriplib.h
	../../common/mathlib.h
	../../common/polylib.h
	../../common/threads.h
	)
set(COMMON_SOURCES
	../../common/bspfile.c
	../../common/cmdlib.c
	../../common/scriplib.c
	../../common/mathlib.c
	../../common/polylib.c
	../../common/threads.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})


#
# bsp generation tool
#

# set include directories
include_directories(
	.
	../../common
	)

# build it
add_executable(qbsp3 ${QBSP_INCLUDES} ${QBSP_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(qbsp3 ${Q_UTILS_DIR})
install(TARGETS qbsp3 RUNTIME DESTINATION ${Q_UTILS_DIR})