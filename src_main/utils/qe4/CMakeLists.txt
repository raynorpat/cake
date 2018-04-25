project(qe4)

# sources
file(GLOB QE4_INCLUDES ./*.h)
file(GLOB QE4_SOURCES ./*.c)
source_group("qe4" FILES ${QE4_INCLUDES})
source_group("qe4" FILES ${QE4_SOURCES})

# shared source
set(SHARED_INCLUDES
	../../shared/q_math.h
	../../shared/piclib.h
	)
set(SHARED_SOURCES
	../../shared/q_math.c
	../../shared/piclib.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})

#
# QuakeEd 4 map editor tool
#

# set include directories
include_directories(
	.
	../../shared
	)

# windows resources
set(WIN32_RESOURCES
	./icon1.ico
	./q.bmp
	./toolbar1.bmp
	./win_qe3.rc
	)
source_group("qe4\\Resources" FILES ${WIN32_RESOURCES})

# build it
add_executable(qe4 WIN32 ${WIN32_RESOURCES} ${SHARED_INCLUDES} ${SHARED_SOURCES} ${QE4_INCLUDES} ${QE4_SOURCES})
target_link_libraries(qe4 PRIVATE opengl32 glu32 comctl32)

q_set_output_dir(qe4 ${Q_UTILS_DIR})
install(TARGETS qe4 RUNTIME DESTINATION ${Q_UTILS_DIR})
