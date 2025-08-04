# DHT Unit Tests CMake Configuration

# Find required packages
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Set up test executable
set(DHT_TEST_SOURCES
    test_node_id.cpp
    test_kademlia_config.cpp
    test_i_routing.cpp
    test_i_storage.cpp
    test_kademlia_routing.cpp
    test_routing_table.cpp
    test_kademlia_protocol.cpp
    test_integration.cpp
)

# Create test executable
add_executable(dht_tests ${DHT_TEST_SOURCES})

# Link libraries
target_link_libraries(dht_tests
    PRIVATE
        ss_p2p
        GTest::gtest
        GTest::gtest_main
        Boost::system
        ${CMAKE_THREAD_LIBS_INIT}
)

# Include directories
target_include_directories(dht_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Compiler flags
target_compile_features(dht_tests PRIVATE cxx_std_20)
target_compile_options(dht_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Test discovery
include(GoogleTest)
gtest_discover_tests(dht_tests)

# Custom test target
add_custom_target(run_dht_tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dht_tests
    DEPENDS dht_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running DHT unit tests"
)