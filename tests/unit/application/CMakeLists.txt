# CMakeLists.txt for Application Unit Tests

# Find required packages
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Test source files
set(APPLICATION_TEST_SOURCES
    test_p2p_node.cpp
    test_message_bus.cpp
)

# Create test executable
add_executable(application_tests ${APPLICATION_TEST_SOURCES})

# Link libraries
target_link_libraries(application_tests
    PRIVATE
    ss_p2p                    # Main library
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

# Compiler settings
target_compile_features(application_tests PRIVATE cxx_std_20)
target_compile_options(application_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0 --coverage>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# Test properties
set_target_properties(application_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/unit/application
)

# Add to CTest
add_test(
    NAME ApplicationUnitTests
    COMMAND application_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/unit/application
)

# Test labels
set_tests_properties(ApplicationUnitTests PROPERTIES
    LABELS "unit;application"
    TIMEOUT 60
)

# Coverage settings (if enabled)
if(ENABLE_COVERAGE)
    target_link_libraries(application_tests PRIVATE --coverage)
endif()

# Install test executable (optional)
install(TARGETS application_tests
    RUNTIME DESTINATION bin/tests/unit/application
    COMPONENT tests
)