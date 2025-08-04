# Test configuration for SS P2P Node Controller

find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Test executable for the new message system
add_executable(test_message_system
    test_message_system.cpp
)

target_link_libraries(test_message_system PRIVATE
    ss_p2p
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
    ${Boost_LIBRARIES}
    Threads::Threads
)

target_include_directories(test_message_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

target_compile_definitions(test_message_system PRIVATE
    SS_DEBUG
    GTEST_HAS_PTHREAD=1
)

# Legacy test executables (if they exist)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_legacy_node_controller.cpp)
    add_executable(test_legacy_node_controller
        test_legacy_node_controller.cpp
    )
    
    target_link_libraries(test_legacy_node_controller PRIVATE
        ss_p2p
        ${Boost_LIBRARIES}
        Threads::Threads
    )
    
    target_include_directories(test_legacy_node_controller PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_compile_definitions(test_legacy_node_controller PRIVATE
        SS_DEBUG
    )
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_legacy_error_handling.cpp)
    add_executable(test_legacy_error_handling
        test_legacy_error_handling.cpp
    )
    
    target_link_libraries(test_legacy_error_handling PRIVATE
        ss_p2p
        ${Boost_LIBRARIES}
        Threads::Threads
    )
    
    target_include_directories(test_legacy_error_handling PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_compile_definitions(test_legacy_error_handling PRIVATE
        SS_DEBUG
    )
endif()

# Register tests with CTest
add_test(NAME MessageSystemTest COMMAND test_message_system)

if(TARGET test_legacy_node_controller)
    add_test(NAME LegacyNodeControllerTest COMMAND test_legacy_node_controller)
endif()

if(TARGET test_legacy_error_handling)
    add_test(NAME LegacyErrorHandlingTest COMMAND test_legacy_error_handling)
endif()

# Test properties for better output
set_tests_properties(MessageSystemTest PROPERTIES
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Coverage reporting (if enabled)
if(ENABLE_COVERAGE)
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
        target_compile_options(test_message_system PRIVATE --coverage)
        target_link_options(test_message_system PRIVATE --coverage)
        
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --list coverage.info
            COMMAND ${GENHTML_PATH} -o coverage coverage.info
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report"
        )
    endif()
endif()

# Memory testing with Valgrind (if available)
find_program(VALGRIND_PATH valgrind)
if(VALGRIND_PATH)
    add_custom_target(memcheck
        COMMAND ${VALGRIND_PATH} --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 $<TARGET_FILE:test_message_system>
        DEPENDS test_message_system
        COMMENT "Running memory checks with Valgrind"
    )
endif()

# Performance benchmarking
add_custom_target(benchmark
    COMMAND $<TARGET_FILE:test_message_system> --gtest_filter="*Performance*"
    DEPENDS test_message_system
    COMMENT "Running performance benchmarks"
)