add_subdirectory(q2map)
add_subdirectory(bake)
add_subdirectory(extractfunctions)

if (MSVC)
	add_subdirectory(qe4)
	add_subdirectory(texpaint)
endif()