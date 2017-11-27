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
	../common/cmdlib.h
	../common/l3dslib.h
	../common/lbmlib.h
	../common/mathlib.h
	../common/scriplib.h
	../common/trilib.h
	)
set(COMMON_SOURCES
	../common/cmdlib.c
	../common/l3dslib.c
	../common/lbmlib.c
	../common/mathlib.c
	../common/scriplib.c
	../common/trilib.c
	)
source_group("common" FILES ${COMMON_INCLUDES})
source_group("common" FILES ${COMMON_SOURCES})


#
# TexPaint tool
#

# set include directories
include_directories(
	.
	../common
	)

# windows resources
set(WIN32_RESOURCES
	./texmake.rc
	)
source_group("texpaint\\Resources" FILES ${WIN32_RESOURCES})

# build it
add_executable(texpaint WIN32 ${WIN32_RESOURCES} ${TEXPAINT_INCLUDES} ${TEXPAINT_SOURCES} ${COMMON_INCLUDES} ${COMMON_SOURCES})
target_link_libraries(texpaint PRIVATE opengl32 glu32)

q_set_output_dir(texpaint "")
install(TARGETS texpaint RUNTIME DESTINATION .)