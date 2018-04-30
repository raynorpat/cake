set(CPACK_PACKAGE_NAME "GTKRadiant")
set(CPACK_PACKAGE_VERSION_MAJOR "${GTKRadiant_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${GTKRadiant_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${GTKRadiant_VERSION_PATCH}")

# binary: --target package
set(CPACK_GENERATOR "ZIP")
set(CPACK_STRIP_FILES 1)

# source: --target package_source
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/;/build/;/install/")

# configure
include(InstallRequiredSystemLibraries)
include(CPack)
