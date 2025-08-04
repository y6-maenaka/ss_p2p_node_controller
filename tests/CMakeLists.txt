# Tests CMakeLists.txt

# Find Google Test or fetch it
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()
include(GoogleTest)

# Test configuration
set(TEST_SOURCES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/unit)

# Helper function to add test
function(add_ss_p2p_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} 
        PRIVATE 
        ss_p2p 
        GTest::gtest_main
        GTest::gmock
    )
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_20)
    gtest_discover_tests(${TEST_NAME})
endfunction()

# Add unit test subdirectories
add_subdirectory(unit/core)
add_subdirectory(unit/network)
add_subdirectory(unit/dht)
add_subdirectory(unit/ice)
add_subdirectory(unit/security)
add_subdirectory(unit/application)

# Integration tests
# add_subdirectory(integration)

# Performance tests
# add_subdirectory(performance)

# Coverage target
if(ENABLE_COVERAGE)
    find_program(LCOV lcov REQUIRED)
    find_program(GENHTML genhtml REQUIRED)
    
    add_custom_target(coverage
        COMMAND ${LCOV} --directory . --zerocounters
        COMMAND ${CMAKE_CTEST_COMMAND}
        COMMAND ${LCOV} --directory . --capture --output-file coverage.info
        COMMAND ${LCOV} --remove coverage.info 
            '/usr/*' 
            '*/googletest/*' 
            '*/tests/*' 
            --output-file coverage.filtered.info
        COMMAND ${GENHTML} coverage.filtered.info --output-directory coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
endif()