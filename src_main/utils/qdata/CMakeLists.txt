project(qdata)

# sources
set(QDATA_INCLUDES
	./anorms.h
	./qdata.h
	)
set(QDATA_SOURCES
	./images.c
	./models.c
	./qdata.c
	./sprites.c
	./tables.c
	./video.c
	)
source_group("qdata" FILES ${QDATA_INCLUDES})
source_group("qdata" FILES ${QDATA_SOURCES})

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
# QDATA tool
#

# set include directories
include_directories(
	.
	../common
	)

# build it
add_executable(qdata ${QDATA_INCLUDES} ${QDATA_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})

q_set_output_dir(qdata ${Q_UTILS_DIR})
install(TARGETS qdata RUNTIME DESTINATION ${Q_UTILS_DIR})