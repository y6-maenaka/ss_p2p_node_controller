# Network module unit tests

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)

# Include test framework (assuming Google Test)
find_package(GTest REQUIRED)
include(GoogleTest)

# Set test sources
set(NETWORK_TEST_SOURCES
    test_transport_interface.cpp
    test_protocol_interface.cpp
    test_message_buffer.cpp
    test_transport_factory.cpp
    test_udp_transport.cpp
    test_tcp_transport.cpp
)

# Create test executable
add_executable(network_tests ${NETWORK_TEST_SOURCES})

# Link libraries
target_link_libraries(network_tests
    PRIVATE
    ss_p2p
    GTest::gtest
    GTest::gtest_main
    Boost::system
    Boost::filesystem
    Threads::Threads
)

# Set include directories
target_include_directories(network_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Set compiler options
target_compile_features(network_tests PRIVATE cxx_std_20)
target_compile_definitions(network_tests PRIVATE
    BOOST_ASIO_HEADER_ONLY
    BOOST_ASIO_SEPARATE_COMPILATION
)

# Add compile options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(network_tests PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        -Wno-unused-variable
    )
endif()

# Register tests with CTest
gtest_discover_tests(network_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    TIMEOUT 60
)

# Custom test targets
add_custom_target(test_network
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R "network_tests"
    DEPENDS network_tests
    COMMENT "Running network unit tests"
)

add_custom_target(test_network_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -R "network_tests" --verbose
    DEPENDS network_tests
    COMMENT "Running network unit tests (verbose)"
)