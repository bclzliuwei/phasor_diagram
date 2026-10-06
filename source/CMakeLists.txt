cmake_minimum_required(VERSION 3.20)
project(PhasorDiagram LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "PhasorDiagram requires Windows and the Win32 API.")
endif()

add_executable(PhasorDiagram WIN32 main.cpp)
target_compile_features(PhasorDiagram PRIVATE cxx_std_17)
target_compile_definitions(PhasorDiagram PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(PhasorDiagram PRIVATE user32 gdi32)

if(MSVC)
    target_compile_options(PhasorDiagram PRIVATE /W4 /permissive-)
endif()