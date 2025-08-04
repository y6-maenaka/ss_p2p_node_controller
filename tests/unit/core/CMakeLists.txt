# CMakeLists.txt for core unit tests

# Find required packages
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Test executable names
set(TEST_TARGETS
    test_types
    test_result
    test_concepts
    test_interfaces
)

# Create test executables
foreach(TEST_TARGET ${TEST_TARGETS})
    add_executable(${TEST_TARGET} ${TEST_TARGET}.cpp)
    
    # Link libraries
    target_link_libraries(${TEST_TARGET}
        ss_p2p                    # Main library
        GTest::GTest
        GTest::Main
        GMock::GMock
        Boost::system
        ${CMAKE_THREAD_LIBS_INIT}
    )
    
    # Set C++20 standard
    target_compile_features(${TEST_TARGET} PRIVATE cxx_std_20)
    
    # Add compiler flags
    target_compile_options(${TEST_TARGET} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
    
    # Add test to CTest
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    
    # Set test properties
    set_tests_properties(${TEST_TARGET} PROPERTIES
        TIMEOUT 30
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# Create a custom target to build all core tests
add_custom_target(core_tests
    DEPENDS ${TEST_TARGETS}
    COMMENT "Building all core unit tests"
)

# Create a custom target to run all core tests
add_custom_target(run_core_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 30
    DEPENDS core_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all core unit tests"
)