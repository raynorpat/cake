project(bspinfo3)

# sources
set(BSPINFO_SOURCES
	./bspinfo3.c
	)
source_group("bspinfo3" FILES ${BSPINFO_SOURCES})

set(COMMON_INCLUDES
	../../common/bspfile.h
	../../common/cmdlib.h
	../../common/scriplib.h
	)
set(COMMON_SOURCES
	../../common/bspfile.c
	../../common/cmdlib.c
	../../common/scriplib.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})


#
# BSP info tool
#

# set include directories
include_directories(
	.
	../../common
	)

# build it
add_executable(bspinfo3 ${BSPINFO_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(bspinfo3 ${Q_UTILS_DIR})
install(TARGETS bspinfo3 RUNTIME DESTINATION ${Q_UTILS_DIR})