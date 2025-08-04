# Examples CMakeLists.txt

# Simple chat example
add_executable(ss_p2p_chat simple_chat/main.cpp)
target_link_libraries(ss_p2p_chat PRIVATE ss_p2p)
target_compile_features(ss_p2p_chat PRIVATE cxx_std_20)

# Set RPATH for installed executables
set_target_properties(ss_p2p_chat PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
)

# Install examples
install(TARGETS ss_p2p_chat
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install example config files
install(FILES simple_chat/config.json.example
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ss_p2p/examples
    OPTIONAL
)

# Add more examples as they are created
# add_subdirectory(file_sharing)
# add_subdirectory(blockchain_node)