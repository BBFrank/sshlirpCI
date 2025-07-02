cmake_minimum_required(VERSION 3.10)
project(sshlirp_ci C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

include_directories(src/include)

set(START_SOURCES
    src/main.c
    src/worker.c
    src/test.c
    src/lib/init/init.c
    src/lib/init/worker_init.c
    src/lib/utils/utils.c
)

set(STOP_SOURCES
    src/stop.c
)

add_executable(sshlirp_ci_start ${START_SOURCES})
add_executable(sshlirp_ci_stop ${STOP_SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(sshlirp_ci_start PRIVATE Threads::Threads)

target_link_options(sshlirp_ci_start PRIVATE "-static")
target_link_options(sshlirp_ci_stop PRIVATE "-static")


message(STATUS "Configuring sshlirp_ci_start and sshlirp_ci_stop for static linking.")
