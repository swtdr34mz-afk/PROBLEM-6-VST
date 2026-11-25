# PROPHET 67's engine tests (engine_tests.cpp): the engine alone, without JUCE.
#   cmake -S tests -B build-tests -DCMAKE_BUILD_TYPE=Release && cmake --build build-tests && build-tests/engine_tests
cmake_minimum_required(VERSION 3.22)
project(P67Tests LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(R ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_executable(engine_tests
    engine_tests.cpp
    ${R}/src/P67Engine.cpp
    ${R}/src/P6Import.cpp
    ${R}/dsp/params.c
    ${R}/dsp/presets.c
    ${R}/src/vst_presets.c)
target_include_directories(engine_tests PRIVATE ${R}/shim ${R}/dsp ${R}/src)
target_compile_definitions(engine_tests PRIVATE P67_PLUGIN=1 PLAT_HOST=1 _USE_MATH_DEFINES)
if(MSVC)
    target_compile_options(engine_tests PRIVATE /O2)
else()
    target_compile_options(engine_tests PRIVATE -O2)
    target_link_libraries(engine_tests PRIVATE m)
endif()
