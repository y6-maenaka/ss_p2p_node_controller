#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ss_p2p::ss_p2p" for configuration ""
set_property(TARGET ss_p2p::ss_p2p APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(ss_p2p::ss_p2p PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libss_p2p.a"
  )

list(APPEND _cmake_import_check_targets ss_p2p::ss_p2p )
list(APPEND _cmake_import_check_files_for_ss_p2p::ss_p2p "${_IMPORT_PREFIX}/lib/libss_p2p.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
