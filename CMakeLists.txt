cmake_minimum_required(VERSION 3.20)

project(OuiPaint VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OUI_PAINT_BUILD_TESTS "Build OUI Paint's test suite" ON)

# OUI, without its own tests and examples, with the desktop window: OUI Paint only runs in a window.
set(OLAGARRO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OLAGARRO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(OLAGARRO_OUI_WINDOW ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/olagarro EXCLUDE_FROM_ALL)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Everything but main(), so the tests reach it too.
add_library(oui_paint_core STATIC
    src/ui/icons.cpp
    src/ui/main_frame.cpp
    src/ui/tool_panel.cpp)
target_include_directories(oui_paint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(oui_paint_core PUBLIC olagarro::OUI olagarro::OUIGraphics)

# The icons the program shows, built into it. Only those it uses: add one here when a button or a menu needs it.
set(OUI_PAINT_ICONS
    Bucket ColorPick CircleShape2D Edit Eraser Gradient Label Line Rectangle RegionEdit ToolMove ToolPan ToolSelect
    Zoom)
list(TRANSFORM OUI_PAINT_ICONS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/icons/godot/)
list(TRANSFORM OUI_PAINT_ICONS APPEND .svg)
olagarro_embed_files(oui_paint_core FUNCTION embedded_icons FILES ${OUI_PAINT_ICONS})

add_executable(oui_paint src/main.cpp)
target_link_libraries(oui_paint PRIVATE oui_paint_core olagarro::OUIWindow)
set_target_properties(oui_paint PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

if(OUI_PAINT_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(oui_paint_tests
        tests/icons_test.cpp
        tests/main_frame_test.cpp
        tests/tool_panel_test.cpp)
    target_link_libraries(oui_paint_tests PRIVATE oui_paint_core GTest::gtest_main)
    gtest_discover_tests(oui_paint_tests)
endif()
