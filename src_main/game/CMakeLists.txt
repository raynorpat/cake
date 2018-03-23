project(game)

# shared code
set(SHARED_INCLUDES
	../shared/game.h
	../shared/q_platform.h
	../shared/q_shared.h
	../shared/qfiles.h
	)
set(SHARED_SOURCES
	../shared/m_flash.c
	../shared/q_shared.c
	)
source_group("shared" FILES ${SHARED_INCLUDES})
source_group("shared" FILES ${SHARED_SOURCES})

# game dll code
file(GLOB GAME_INCLUDES *.h)
file(GLOB GAME_SOURCES *.c)
source_group("game" FILES ${GAME_INCLUDES})
source_group("game" FILES ${GAME_SOURCES})

# set include directories
include_directories(
			.
			../shared
            )

# build the game dll
add_library(game SHARED ${SHARED_INCLUDES} ${SHARED_SOURCES} ${GAME_INCLUDES} ${GAME_SOURCES})
target_link_libraries(game PRIVATE)
q_set_output_dir(game ${Q_GAME_DIR})
install(TARGETS game RUNTIME DESTINATION ${Q_GAME_DIR} LIBRARY DESTINATION ${Q_GAME_DIR})
