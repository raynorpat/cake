project(bake)

# libogg & libvorbis
find_package(OGG)
if (OGG_FOUND)
	include_directories(${OGG_INCLUDE_DIRS})
	set(OGG_LIBRARY ${OGG_LIBRARIES})
	set(OGG_INCLUDES "")
	set(OGG_SOURCES "")
else (OGG_FOUND)
	include_directories("../../libs/libogg/include")
	set(OGG_LIBRARY "")
	file(GLOB OGG_INCLUDES ../../libs/libogg/include/ogg/*.h)
	file(GLOB OGG_SOURCES ../../libs/libogg/src/*.c)
endif (OGG_FOUND)
source_group("libs\\libogg" FILES ${OGG_INCLUDES})
source_group("libs\\libogg" FILES ${OGG_SOURCES})

find_package(VORBIS)
if (VORBIS_FOUND)
	include_directories(${VORBIS_INCLUDE_DIRS})
	set(VORBIS_LIBRARY ${VORBIS_LIBRARIES})
	set(VORBIS_INCLUDES "")
	set(VORBIS_SOURCES "")
else (VORBIS_FOUND)
	include_directories("../../libs/libvorbis/include")
	set(VORBIS_LIBRARY "")
	file(GLOB VORBIS_INCLUDES ../../libs/libvorbis/include/vorbis/*.h
								../../libs/libvorbis/lib/modes/*.h
								../../libs/libvorbis/lib/books/uncoupled/*.h
								../../libs/libvorbis/lib/books/floor/*.h
								../../libs/libvorbis/lib/books/coupled/*.h)
	file(GLOB VORBIS_SOURCES ../../libs/libvorbis/lib/*.c)
endif (VORBIS_FOUND)
source_group("libs\\libvorbis" FILES ${VORBIS_INCLUDES})
source_group("libs\\libvorbis" FILES ${VORBIS_SOURCES})	

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
	./video.c
	./music.c
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

set(ROQ_INCLUDES
	./roq/audio.h
	./roq/codebook.h
	./roq/common.h
	./roq/elbg.h
	./roq/random.h
	./roq/roq.h
	./roq/video.h
	./roq_encoder.h
	)
set(ROQ_SOURCES
	./roq/audio.cpp
	./roq/codebook.cpp
	./roq/common.cpp
	./roq/elbg.cpp
	./roq/random.cpp
	./roq/video.cpp
	./roq_encoder.cpp
	)
source_group("roq" FILES ${ROQ_INCLUDES})
source_group("roq" FILES ${ROQ_SOURCES})



#
# BAKE tool
#

# set include directories
include_directories(
	.
	../common
	)

# build it
add_executable(bake ${OGG_INCLUDES} ${OGG_SOURCES} ${VORBIS_INCLUDES} ${VORBIS_SOURCES} 
					${BAKE_INCLUDES} ${BAKE_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES}
					${ROQ_INCLUDES} ${ROQ_SOURCES})

q_set_output_dir(bake ${Q_UTILS_DIR})
install(TARGETS bake RUNTIME DESTINATION ${Q_UTILS_DIR})