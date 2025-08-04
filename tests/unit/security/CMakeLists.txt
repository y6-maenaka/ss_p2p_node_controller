# Security module unit tests
cmake_minimum_required(VERSION 3.18)

# Find required packages
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Test executables
set(SECURITY_TESTS
    test_crypto
    test_auth
    test_key_manager
    test_secure_channel
    test_integration
)

# Crypto tests
add_executable(test_crypto
    test_crypto.cpp
    ${CMAKE_SOURCE_DIR}/src/ss_p2p/core/types.cpp
)

target_link_libraries(test_crypto
    GTest::gtest
    GTest::gtest_main
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    pthread
)

target_compile_features(test_crypto PRIVATE cxx_std_20)
target_compile_definitions(test_crypto PRIVATE
    DEFAULT_LOGGER_OUTFILE_DIR="${CMAKE_SOURCE_DIR}/log"
)

# Authentication tests
add_executable(test_auth
    test_auth.cpp
    ${CMAKE_SOURCE_DIR}/src/ss_p2p/core/types.cpp
)

target_link_libraries(test_auth
    GTest::gtest
    GTest::gtest_main
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    pthread
)

target_compile_features(test_auth PRIVATE cxx_std_20)
target_compile_definitions(test_auth PRIVATE
    DEFAULT_LOGGER_OUTFILE_DIR="${CMAKE_SOURCE_DIR}/log"
)

# Key manager tests
add_executable(test_key_manager
    test_key_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/ss_p2p/core/types.cpp
)

target_link_libraries(test_key_manager
    GTest::gtest
    GTest::gtest_main
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    pthread
)

target_compile_features(test_key_manager PRIVATE cxx_std_20)
target_compile_definitions(test_key_manager PRIVATE
    DEFAULT_LOGGER_OUTFILE_DIR="${CMAKE_SOURCE_DIR}/log"
)

# Secure channel tests
add_executable(test_secure_channel
    test_secure_channel.cpp
    ${CMAKE_SOURCE_DIR}/src/ss_p2p/core/types.cpp
)

target_link_libraries(test_secure_channel
    GTest::gtest
    GTest::gtest_main
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    pthread
)

target_compile_features(test_secure_channel PRIVATE cxx_std_20)
target_compile_definitions(test_secure_channel PRIVATE
    DEFAULT_LOGGER_OUTFILE_DIR="${CMAKE_SOURCE_DIR}/log"
)

# Integration tests
add_executable(test_integration
    test_integration.cpp
    ${CMAKE_SOURCE_DIR}/src/ss_p2p/core/types.cpp
)

target_link_libraries(test_integration
    GTest::gtest
    GTest::gtest_main
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    pthread
)

target_compile_features(test_integration PRIVATE cxx_std_20)
target_compile_definitions(test_integration PRIVATE
    DEFAULT_LOGGER_OUTFILE_DIR="${CMAKE_SOURCE_DIR}/log"
)

# Add tests to CTest
include(GoogleTest)

foreach(test_name ${SECURITY_TESTS})
    gtest_discover_tests(${test_name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        PROPERTIES
            TIMEOUT 60
            LABELS "security;unit"
    )
endforeach()

# Custom target to run all security tests
add_custom_target(test_security
    COMMAND ${CMAKE_CTEST_COMMAND} --label-regex "security"
    DEPENDS ${SECURITY_TESTS}
    COMMENT "Running all security tests"
)