add_library(uilib
        uilib.cpp
        )

find_package(GLIB REQUIRED)
target_include_directories(uilib PUBLIC ${GLIB_INCLUDE_DIRS})
target_link_libraries(uilib PUBLIC ${GLIB_LIBRARIES})

find_package(GTK2 2.10 REQUIRED gtk)
target_include_directories(uilib PUBLIC ${GTK2_INCLUDE_DIRS})
target_link_libraries(uilib PUBLIC ${GTK2_LIBRARIES})

target_include_directories(uilib PUBLIC gtkutil)
target_link_libraries(uilib PUBLIC gtkutil)