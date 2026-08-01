cmake_minimum_required(VERSION 3.16)
project(archisys CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    src/component.cpp
    src/simulation_engine.cpp
)

# ── CLI tool ──────────────────────────────────────────────────────────────────
add_executable(archisys_sim ${COMMON_SOURCES} src/main.cpp)
target_include_directories(archisys_sim PRIVATE include include/components third_party)

# ── API Server ────────────────────────────────────────────────────────────────
add_executable(archisys_server
    ${COMMON_SOURCES}
    src/http_server.cpp
    src/ws_server.cpp
    src/server_main.cpp
)
target_include_directories(archisys_server PRIVATE include include/components third_party)

# ── Platform-specific ────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(archisys_sim    PRIVATE /W3 /wd4996)
    target_compile_options(archisys_server PRIVATE /W3 /wd4996)
    target_link_libraries(archisys_server  PRIVATE ws2_32)
else()
    target_compile_options(archisys_sim    PRIVATE -Wall -Wextra -O2)
    target_compile_options(archisys_server PRIVATE -Wall -Wextra -O2)
    if(WIN32)
        # MinGW needs explicit POSIX thread model and ws2_32
        target_compile_options(archisys_server PRIVATE -mthreads)
        target_link_libraries(archisys_server  PRIVATE ws2_32 -mthreads)
        target_compile_options(archisys_sim    PRIVATE -mthreads)
    endif()
endif()
