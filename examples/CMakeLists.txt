# Examples CMakeLists.txt

# Modern P2P Chat example using new APIs
add_executable(ss_chat ss_chat/main.cpp)
target_link_libraries(ss_chat PRIVATE 
    ss_p2p
    Boost::system
    Boost::coroutine
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_compile_features(ss_chat PRIVATE cxx_std_20)

# Enable coroutines support
target_compile_definitions(ss_chat PRIVATE 
    BOOST_ASIO_HAS_CO_AWAIT
    BOOST_ASIO_HAS_STD_COROUTINE
)

# Set compiler-specific coroutine flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ss_chat PRIVATE -fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ss_chat PRIVATE -fcoroutines-ts)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ss_chat PRIVATE /await)
endif()

# Set RPATH for installed executables
set_target_properties(ss_chat PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Install examples
install(TARGETS ss_chat
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install example config files
install(FILES ss_chat/config.json.example
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ss_p2p/examples
    OPTIONAL
)

# Add example description
set_target_properties(ss_chat PROPERTIES
    DESCRIPTION "Modern P2P Chat Application using SS P2P APIs"
    VERSION "2.0.0"
)

