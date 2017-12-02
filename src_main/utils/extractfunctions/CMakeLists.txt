project(extractfuncts)

# sources
set(EXTRACTFUNCTS_INCLUDES
	./ef_local.h
	./l_log.h
	./l_memory.h
	./l_precomp.h
	./l_script.h
	)
set(EXTRACTFUNCTS_SOURCES
	./extractfuncs.c
	./l_log.c
	./l_memory.c
	./l_precomp.c
	./l_script.c
	)
source_group("extractfuncts" FILES ${EXTRACTFUNCTS_INCLUDES})
source_group("extractfuncts" FILES ${EXTRACTFUNCTS_SOURCES})

#
# extractfuncts save game table generator tool
#

# set include directories
include_directories(
	.
	)

add_definitions(-DSCREWUP)

# build it
add_executable(extractfuncts ${EXTRACTFUNCTS_INCLUDES} ${EXTRACTFUNCTS_SOURCES})

q_set_output_dir(extractfuncts ${Q_UTILS_DIR})
install(TARGETS extractfuncts RUNTIME DESTINATION ${Q_UTILS_DIR})