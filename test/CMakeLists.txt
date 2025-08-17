set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(unit_tests
    server_test.cpp
    ../src/apps.c
    ../src/cmd.c
    ../src/config.c
    ../src/filecmd.c
    ../src/heartbeat.c
    ../src/ini.c
    ../src/log.c
    ../src/process.c
    ../src/server.c
    ../src/stats.c
    ../src/test.c
    ../src/utils.c
)
target_link_libraries(
    unit_tests 
    PRIVATE 
    GTest::GTest 
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
