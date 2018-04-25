project(texpaint)

# sources
set(TEXPAINT_INCLUDES
	./resource.h
	./texpaint.h
	)
set(TEXPAINT_SOURCES
	./texpaint.c
	./win_cam.c
	./win_main.c
	./win_pal.c
	./win_skin.c
	)
source_group("texpaint" FILES ${TEXPAINT_INCLUDES})
source_group("texpaint" FILES ${TEXPAINT_SOURCES})

set(COMMON_INCLUDES
	../common/l3dslib.h
	../common/scriplib.h
	../common/trilib.h
	)
set(COMMON_SOURCES
	../common/l3dslib.c
	../common/scriplib.c
	../common/trilib.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})

set(SHARED_INCLUDES
	../../shared/q_math.h
	../../shared/piclib.h
	../../shared/cmdlib.h
	)
set(SHARED_SOURCES
	../../shared/q_math.c
	../../shared/piclib.c
	../../shared/cmdlib.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})


#
# TexPaint tool
#

# set include directories
include_directories(
	.
	../common
	../../shared
	)

# windows resources
set(WIN32_RESOURCES
	./texmake.rc
	)
source_group("texpaint\\Resources" FILES ${WIN32_RESOURCES})

# build it
add_executable(texpaint WIN32 ${WIN32_RESOURCES} ${TEXPAINT_INCLUDES} ${TEXPAINT_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES} ${SHARED_INCLUDES} ${SHARED_SOURCES})
target_link_libraries(texpaint PRIVATE opengl32 glu32)

q_set_output_dir(texpaint ${Q_UTILS_DIR})
install(TARGETS texpaint RUNTIME DESTINATION ${Q_UTILS_DIR})