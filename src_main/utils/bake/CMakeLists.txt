project(bake)

# sources
set(BAKE_INCLUDES
	./anorms.h
	./bake.h
	)
set(BAKE_SOURCES
	./images.c
	./models.c
	./bake.c
	./sprites.c
	./tables.c
	./video.c
	)
source_group("bake" FILES ${BAKE_INCLUDES})
source_group("bake" FILES ${BAKE_SOURCES})

set(COMMON_INCLUDES
	../common/bspfile.h
	../common/cmdlib.h
	../common/l3dslib.h
	../common/lbmlib.h
	../common/mathlib.h
	../common/mdfour.h
	../common/scriplib.h
	../common/trilib.h
	../common/threads.h
	)
set(COMMON_SOURCES
	../common/bspfile.c
	../common/cmdlib.c
	../common/l3dslib.c
	../common/lbmlib.c
	../common/mathlib.c
	../common/mdfour.c
	../common/scriplib.c
	../common/trilib.c
	../common/threads.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})


#
# BAKE tool
#

# set include directories
include_directories(
	.
	../common
	)

# build it
add_executable(bake ${BAKE_INCLUDES} ${BAKE_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(bake ${Q_UTILS_DIR})
install(TARGETS bake RUNTIME DESTINATION ${Q_UTILS_DIR})