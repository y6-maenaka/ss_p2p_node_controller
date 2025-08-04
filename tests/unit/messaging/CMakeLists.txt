# Messaging module tests

include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50f33f9142cc5c5684bd01602a86fd.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test executable for the new message system
add_executable(test_message_system
    test_message_system.cpp
)

target_link_libraries(test_message_system PRIVATE
    ss_p2p
    gtest_main
    gmock_main
    ${Boost_LIBRARIES}
    Threads::Threads
)

target_include_directories(test_message_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_compile_definitions(test_message_system PRIVATE
    SS_DEBUG
    GTEST_HAS_PTHREAD=1
)

# Register with CTest
add_test(NAME MessageSystemTest COMMAND test_message_system)

set_tests_properties(MessageSystemTest PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)