add_subdirectory(bsp)
add_subdirectory(qdata)

if (MSVC)
	add_subdirectory(qe4)
	add_subdirectory(texpaint)
endif()