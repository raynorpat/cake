project(qrad3)

# sources
set(QRAD_INCLUDES
	./qrad.h
	)
set(QRAD_SOURCES
	./lightmap.c
	./patches.c
	./qrad3.c
	./trace.c
	)
source_group("qrad3" FILES ${QRAD_INCLUDES})
source_group("qrad3" FILES ${QRAD_SOURCES})

set(COMMON_INCLUDES
	../../common/bspfile.h
	../../common/cmdlib.h
	../../common/scriplib.h
	../../common/lbmlib.h
	../../common/mathlib.h
	../../common/polylib.h
	../../common/threads.h
	)
set(COMMON_SOURCES
	../../common/bspfile.c
	../../common/cmdlib.c
	../../common/scriplib.c
	../../common/lbmlib.c
	../../common/mathlib.c
	../../common/polylib.c
	../../common/threads.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})


#
# radiosity tool
#

# set include directories
include_directories(
	.
	../../common
	)

# build it
add_executable(qrad3 ${QRAD_INCLUDES} ${QRAD_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(qrad3 "")
install(TARGETS qrad3 RUNTIME DESTINATION .)