# CMake configuration for ICE unit tests

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED zlib)

# ICE unit test sources
set(ICE_TEST_SOURCES
    test_ice_candidate.cpp
    test_stun_client.cpp
    test_turn_relay.cpp
    test_nat_traversal.cpp
    test_ice_integration.cpp
)

# Create test executable
add_executable(ice_unit_tests ${ICE_TEST_SOURCES})

# Link libraries
target_link_libraries(ice_unit_tests
    PRIVATE
        ss_p2p
        GTest::gtest
        GTest::gtest_main
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        ${ZLIB_LIBRARIES}
)

# Include directories
target_include_directories(ice_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${ZLIB_INCLUDE_DIRS}
)

# Compiler features
target_compile_features(ice_unit_tests PRIVATE cxx_std_20)

# Compiler definitions
target_compile_definitions(ice_unit_tests
    PRIVATE
        BOOST_ASIO_HAS_CO_AWAIT
        BOOST_ASIO_HAS_STD_COROUTINE
)

# Add test to CTest
add_test(NAME ICEUnitTests COMMAND ice_unit_tests)

# Set test properties
set_tests_properties(ICEUnitTests PROPERTIES
    TIMEOUT 30
    LABELS "unit;ice"
)

# Test-specific compiler flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ice_unit_tests PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# MSVC specific flags
if(MSVC)
    target_compile_options(ice_unit_tests PRIVATE
        /W4
        /wd4996  # Disable deprecation warnings
    )
endif()

# Enable parallel test execution
if(DEFINED ENV{PARALLEL_TESTS})
    set_property(TEST ICEUnitTests PROPERTY PARALLEL_LEVEL $ENV{PARALLEL_TESTS})
endif()

# Code coverage support
if(ENABLE_COVERAGE)
    target_compile_options(ice_unit_tests PRIVATE --coverage)
    target_link_options(ice_unit_tests PRIVATE --coverage)
endif()

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
        option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
        option(ENABLE_UBSAN "Enable UBSanitizer" OFF)
        
        if(ENABLE_ASAN)
            target_compile_options(ice_unit_tests PRIVATE -fsanitize=address)
            target_link_options(ice_unit_tests PRIVATE -fsanitize=address)
        endif()
        
        if(ENABLE_TSAN)
            target_compile_options(ice_unit_tests PRIVATE -fsanitize=thread)
            target_link_options(ice_unit_tests PRIVATE -fsanitize=thread)
        endif()
        
        if(ENABLE_UBSAN)
            target_compile_options(ice_unit_tests PRIVATE -fsanitize=undefined)
            target_link_options(ice_unit_tests PRIVATE -fsanitize=undefined)
        endif()
    endif()
endif()