cmake_minimum_required(VERSION 3.10)
project(processWatchdog C CXX)

option(MAX_APPS_OPTION "Set the maximum number of applications" ON)
if(MAX_APPS_OPTION)
    set(MAX_APPS 6 CACHE STRING "Maximum number of applications")
    add_definitions(-DMAX_APPS=${MAX_APPS})
endif()

# Add the include directory
include_directories(${PROJECT_SOURCE_DIR}/src)

# Add the main executable
add_executable(processWatchdog
    src/main.c
    src/apps.c
    src/cmd.c
    src/config.c
    src/filecmd.c
    src/heartbeat.c
    src/ini.c
    src/log.c
    src/process.c
    src/server.c
    src/stats.c
    src/test.c
    src/utils.c
)

# Add the unit tests
enable_testing()
find_package(GTest REQUIRED)
add_subdirectory(test)
