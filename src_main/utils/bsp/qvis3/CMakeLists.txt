project(qvis3)

# sources
set(QVIS_INCLUDES
	./vis.h
	)
set(QVIS_SOURCES
	./flow.c
	./qvis3.c
	)
source_group("qvis3" FILES ${QVIS_INCLUDES})
source_group("qvis3" FILES ${QVIS_SOURCES})

set(COMMON_INCLUDES
	../../common/bspfile.h
	../../common/cmdlib.h
	../../common/scriplib.h
	../../common/mathlib.h
	../../common/threads.h
	)
set(COMMON_SOURCES
	../../common/bspfile.c
	../../common/cmdlib.c
	../../common/scriplib.c
	../../common/mathlib.c
	../../common/threads.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})


#
# bsp visibility generation tool
#

# set include directories
include_directories(
	.
	../../common
	)

# build it
add_executable(qvis3 ${QVIS_INCLUDES} ${QVIS_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(qvis3 ${Q_UTILS_DIR})
install(TARGETS qvis3 RUNTIME DESTINATION ${Q_UTILS_DIR})