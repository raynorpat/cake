# - Find vorbis library
# Find the native Vorbis headers and libraries.
#
#  VORBIS_INCLUDE_DIRS   - where to find vorbis/vorbisfile.h, etc
#  VORBIS_LIBRARIES      - List of libraries when using libvorbis
#  VORBIS_FOUND          - True if vorbis is found.

# Look for the vorbisfile header file.
find_path( VORBIS_INCLUDE_DIR
  NAMES vorbis/vorbisfile.h
  DOC "Vorbis include directory" )
mark_as_advanced( VORBIS_INCLUDE_DIR )

# Look for the vorbisfile library.
find_library( VORBISFILE_LIBRARY
  NAMES vorbisfile
  DOC "Path to VorbisFile library" )
mark_as_advanced( VORBISFILE_LIBRARY )

# Look for the vorbis library.
find_library( VORBIS_LIBRARY
  NAMES vorbis
  DOC "Path to Vorbis library" )
mark_as_advanced( VORBIS_LIBRARY )

# handle the QUIETLY and REQUIRED arguments and set VORBISFILE_FOUND to TRUE if 
# all listed variables are TRUE
include( ${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( Vorbis DEFAULT_MSG VORBIS_LIBRARY VORBIS_INCLUDE_DIR )

set( VORBIS_LIBRARIES ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} )
set( VORBIS_INCLUDE_DIRS ${VORBIS_INCLUDE_DIR} )
