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

# lodepng writes PNG at 16 bits, which stb cannot; it is used as it comes, without OUI Paint's warnings.
add_library(oui_paint_lodepng STATIC third_party/lodepng/lodepng.cpp)
target_include_directories(oui_paint_lodepng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lodepng)
target_compile_options(oui_paint_lodepng PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)

# Everything but main(), so the tests reach it too.
add_library(oui_paint_core STATIC
    src/codecs/codecs.cpp
    src/codecs/stb_api.cpp
    src/core/document.cpp
    src/core/image.cpp
    src/core/layer.cpp
    src/core/pixel_format.cpp
    src/core/tile.cpp
    src/ui/icons.cpp
    src/ui/main_frame.cpp
    src/ui/tool_panel.cpp)
target_include_directories(oui_paint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(oui_paint_core PUBLIC olagarro::OUI olagarro::OUIGraphics PRIVATE oui_paint_lodepng)
target_include_directories(oui_paint_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
# stb is used as it comes, so OUI Paint's warnings stay off it.
set_source_files_properties(src/codecs/stb_api.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")

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
        tests/codecs_test.cpp
        tests/core_test.cpp
        tests/icons_test.cpp
        tests/main_frame_test.cpp
        tests/tool_panel_test.cpp)
    target_link_libraries(oui_paint_tests PRIVATE oui_paint_core GTest::gtest_main)
    gtest_discover_tests(oui_paint_tests)
endif()
