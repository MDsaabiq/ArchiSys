cmake_minimum_required(VERSION 3.16)
project(archisys_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Core C++ Sources & Includes ───────────────────────────────────────────────
set(ENGINE_SOURCES
    src/component.cpp
    src/simulation_engine.cpp
    src/simulator.cpp
)

set(ENGINE_INCLUDES
    include
    third_party
)

# ── Find Python and pybind11 ──────────────────────────────────────────────────
find_package(Python 3.8 COMPONENTS Interpreter Development)
if(Python_FOUND)
    message(STATUS "Found Python: ${Python_EXECUTABLE} (version ${Python_VERSION})")
endif()

find_package(pybind11 CONFIG)
if(NOT pybind11_FOUND)
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE PYBIND11_DIR_RES
    )
    if(PYBIND11_DIR_RES EQUAL 0 AND EXISTS "${PYBIND11_CMAKE_DIR}")
        list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_CMAKE_DIR}")
        find_package(pybind11 CONFIG REQUIRED)
    endif()
endif()

# ── pybind11 Python Native Extension (archisys_cpp) ───────────────────────────
if(pybind11_FOUND)
    pybind11_add_module(archisys_cpp
        ${ENGINE_SOURCES}
        src/bindings.cpp
    )
    target_include_directories(archisys_cpp PRIVATE ${ENGINE_INCLUDES})

    if(MSVC)
        target_compile_options(archisys_cpp PRIVATE /W3 /wd4996)
    else()
        target_compile_options(archisys_cpp PRIVATE -Wall -Wextra -O3)
        if(WIN32)
            target_compile_options(archisys_cpp PRIVATE -mthreads)
            target_link_libraries(archisys_cpp PRIVATE -mthreads -static -static-libgcc -static-libstdc++)
        endif()
    endif()
endif()
