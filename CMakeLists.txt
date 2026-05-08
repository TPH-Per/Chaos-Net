cmake_minimum_required(VERSION 3.16)
project(chaosnet C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -pedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
pkg_check_modules(MHD       REQUIRED libmicrohttpd)

include_directories(include)

set(SOURCES
    src/main.c
    src/logger.c
    src/observer.c
    src/injector_net.c
    src/injector_mem.c
    src/injector_cpu.c
    src/control_http.c
    src/control_mqtt.c
    src/scenario.c
    src/report.c
)

add_executable(chaosnet ${SOURCES})

target_link_libraries(chaosnet
    ${MOSQUITTO_LIBRARIES}
    ${MHD_LIBRARIES}
    pthread
    m
)

target_include_directories(chaosnet PRIVATE
    ${MOSQUITTO_INCLUDE_DIRS}
    ${MHD_INCLUDE_DIRS}
)

# Target test app
add_subdirectory(tools/target_app)

# Tests
enable_testing()
add_subdirectory(tests)
