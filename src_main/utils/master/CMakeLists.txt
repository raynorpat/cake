project(master)

# libcurl
find_package(CURL)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    set(CURL_LIBRARY ${CURL_LIBRARIES})
	add_definitions(-DUSE_CURL -DCURL_STATICLIB -DCURL_DISABLE_LDAP)
else (CURL_FOUND)
    include_directories("../../libs/libcurl/include")
	include_directories("../../libs/libcurl/lib")
    set(CURL_LIBRARY "" )
	message(STATUS "Using builtin libcurl")
	add_definitions(-DUSE_CURL)
endif (CURL_FOUND)

# libcurl sources
if (NOT CURL_FOUND)
	file(GLOB CURL_INCLUDES ../../libs/libcurl/include/curl/*.h)
	file(GLOB CURL_SOURCES ../../libs/libcurl/lib/*.c ../../libs/libcurl/lib/vauth/*.c ../../libs/libcurl/lib/vtls/*.c)
	add_definitions(-DCURL_STATICLIB
					-DBUILDING_LIBCURL 
					-DCURL_DISABLE_LDAP -DCURL_DISABLE_CRYPTO_AUTH)
else (NOT CURL_FOUND)
    set(CURL_INCLUDES "")
    set(CURL_SOURCES "")
endif (NOT CURL_FOUND)
source_group("libs\\curl" FILES ${CURL_INCLUDES})
source_group("libs\\curl" FILES ${CURL_SOURCES})

# master sources
set(MASTER_INCLUDES
	./curl_dl.h
	./dg_misc.h
	./dk_essentials.h
	./master.h
	./performance.h
	./service.h
	./shared.h
	)
set(MASTER_SOURCES
	./curl_dl.cpp
	./dk_essentials.cpp
	./enctype_shared.cpp
	./enctype1_decoder.cpp
	./gamestable.cpp
	./gsmalg.cpp
	./master.cpp
	./performance.cpp
	./service.cpp
	)
source_group("master" FILES ${MASTER_INCLUDES})
source_group("master" FILES ${MASTER_SOURCES})


#
# multiplayer master server tool
#

# set include directories
include_directories(
	.
	)

# build it
add_executable(master 
				${CURL_INCLUDES} ${CURL_SOURCES}
				${MASTER_INCLUDES} ${MASTER_SOURCES}
				)
				
target_link_libraries(master ${CURL_LIBRARY})

q_set_output_dir(master ${Q_UTILS_DIR})
install(TARGETS master RUNTIME DESTINATION ${Q_UTILS_DIR})
