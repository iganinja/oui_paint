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

# miniz zips OpenRaster files, used as it comes.
add_library(oui_paint_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(oui_paint_miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz)
target_compile_options(oui_paint_miniz PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)

# Everything but main(), so the tests reach it too.
add_library(oui_paint_core STATIC
    src/app/file_filters.cpp
    src/app/background_effect.cpp
    src/app/fonts.cpp
    src/app/help.cpp
    src/app/new_image.cpp
    src/app/open_document.cpp
    src/app/plugin_api.cpp
    src/app/paint_app.cpp
    src/app/recent_files.cpp
    src/app/tools.cpp
    src/app/translations.cpp
    src/app/window_place.cpp
    src/codecs/codecs.cpp
    src/codecs/openraster.cpp
    src/codecs/palettes.cpp
    src/codecs/stb_api.cpp
    src/codecs/zip_api.cpp
    src/core/color.cpp
    src/core/document.cpp
    src/core/json.cpp
    src/core/history.cpp
    src/core/image.cpp
    src/core/layer.cpp
    src/core/palette.cpp
    src/core/pixel_format.cpp
    src/core/selection.cpp
    src/core/tile.cpp
    src/core/transform.cpp
    src/effects/curve_edit.cpp
    src/effects/curves.cpp
    src/effects/indexing.cpp
    src/effects/mode_change.cpp
    src/effects/pieces.cpp
    src/effects/plugin_trial.cpp
    src/effects/plugins.cpp
    src/effects/test_card.cpp
    src/render/display_cache.cpp
    src/render/viewport.cpp
    src/tools/color_math.cpp
    src/tools/fill.cpp
    src/tools/floating.cpp
    src/tools/gradient.cpp
    src/tools/arc.cpp
    src/tools/shapes.cpp
    src/tools/stroke.cpp
    src/tools/text.cpp
    src/ui/canvas_view.cpp
    src/ui/anchor_grid.cpp
    src/ui/color_dialog.cpp
    src/ui/color_panel.cpp
    src/ui/curve_editor.cpp
    src/ui/curves_dialog.cpp
    src/ui/effect_controls.cpp
    src/ui/effect_dialog.cpp
    src/ui/help_dialogs.cpp
    src/ui/history_panel.cpp
    src/ui/icons.cpp
    src/ui/layer_properties_dialog.cpp
    src/ui/layers_panel.cpp
    src/ui/levels_bar.cpp
    src/ui/levels_dialog.cpp
    src/ui/light_dialog.cpp
    src/ui/mode_dialogs.cpp
    src/ui/main_frame.cpp
    src/ui/new_image_dialog.cpp
    src/ui/palettes_panel.cpp
    src/ui/preferences_dialog.cpp
    src/ui/shown_names.cpp
    src/ui/plugin_workbench.cpp
    src/ui/plugins_dialog.cpp
    src/ui/tool_options.cpp
    src/ui/transform_dialogs.cpp
    src/ui/tool_panel.cpp)
target_include_directories(oui_paint_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(oui_paint_core PUBLIC olagarro::OUI olagarro::OUIGraphics olagarro::OUIScript
    PRIVATE olagarro::Concurrency oui_paint_lodepng oui_paint_miniz)
target_include_directories(oui_paint_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
# stb and miniz are used as they come, so OUI Paint's warnings stay off the files that hold them.
set_source_files_properties(src/codecs/stb_api.cpp src/codecs/zip_api.cpp
    PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>")

# The icons the program shows, built into it. Only those it uses: add one here when a button or a menu needs it.
set(OUI_PAINT_GODOT_ICONS
    Add Bucket ColorPick Duplicate Edit Eraser Gradient GuiVisibilityHidden GuiVisibilityVisible Image Line MoveDown
    MoveUp RegionEdit Remove RotateLeft RotateRight ToolMove ToolPan ToolSelect Tools Zoom)
list(TRANSFORM OUI_PAINT_GODOT_ICONS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/icons/godot/)
set(OUI_PAINT_OWN_ICONS Arc Bold CloneStamp EllipseSelect Italic Lasso Light MagicWand MergeDown Paintbrush Recolor
    Shapes Strikethrough Text)
list(TRANSFORM OUI_PAINT_OWN_ICONS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/icons/oui_paint/)
set(OUI_PAINT_ICONS ${OUI_PAINT_GODOT_ICONS} ${OUI_PAINT_OWN_ICONS})
list(TRANSFORM OUI_PAINT_ICONS APPEND .svg)
olagarro_embed_files(oui_paint_core FUNCTION embedded_icons FILES ${OUI_PAINT_ICONS})
# The plugins built in, the prelude every plugin runs after first.
olagarro_embed_files(oui_paint_core FUNCTION embedded_plugins
    FILES src/plugins/prelude.js src/plugins/adjustments.js src/plugins/effects.js src/plugins/indexers.js)
# What plugins can use, which the workbench suggests from and the help lists.
olagarro_embed_files(oui_paint_core NAMESPACE oui_paint FUNCTION embedded_plugin_api FILES src/plugins/paint.d.ts)
# The help, and OUI Paint's license, which About shows.
olagarro_embed_files(oui_paint_core NAMESPACE oui_paint FUNCTION embedded_help
    FILES doc/guide.md doc/guide.es.md doc/plugins.md LICENSE)
olagarro_embed_files(oui_paint_core NAMESPACE oui_paint FUNCTION embedded_catalogs FILES translations/es.json)
# The licenses of what the program carries, which a copy of it has to carry: in the order help.cpp names them.
set(OUI_PAINT_NOTICES
    third_party/olagarro/LICENSE
    third_party/olagarro/third_party/quickjs-ng/LICENSE
    third_party/olagarro/third_party/nanosvg/LICENSE.txt
    third_party/stb/LICENSE
    third_party/lodepng/LICENSE
    third_party/miniz/LICENSE
    third_party/olagarro/licenses/SDL3.txt
    third_party/olagarro/oui/fonts/spleen_license.txt
    third_party/olagarro/oui/fonts/jersey10_ofl.txt
    icons/godot/LICENSE.txt)
if(WIN32)
    list(APPEND OUI_PAINT_NOTICES third_party/olagarro/licenses/mingw-w64-runtime.txt
        third_party/olagarro/licenses/winpthreads.txt)
endif()
olagarro_embed_files(oui_paint_core NAMESPACE oui_paint FUNCTION embedded_notices FILES ${OUI_PAINT_NOTICES})
# The version About shows: a release's, from its tag, or the project's.
set(OUI_PAINT_VERSION "${PROJECT_VERSION}" CACHE STRING "The version OUI Paint shows")
target_compile_definitions(oui_paint_core PRIVATE OUI_PAINT_VERSION="${OUI_PAINT_VERSION}")

add_executable(oui_paint src/main.cpp)
target_link_libraries(oui_paint PRIVATE oui_paint_core olagarro::OUIWindow)
# bin/, unless the build says where its programs go.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(oui_paint PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()
if(WIN32)
    # A window and nothing else: no console beside it, and every runtime inside, so it needs nothing installed.
    set_target_properties(oui_paint PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(oui_paint PRIVATE -static)
endif()

if(OUI_PAINT_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(oui_paint_tests
        tests/anchor_grid_test.cpp
        tests/codecs_test.cpp
        tests/color_panel_test.cpp
        tests/color_math_test.cpp
        tests/core_test.cpp
        tests/curve_edit_test.cpp
        tests/curves_test.cpp
        tests/file_filters_test.cpp
        tests/fill_test.cpp
        tests/floating_test.cpp
        tests/fonts_test.cpp
        tests/gradient_test.cpp
        tests/help_test.cpp
        tests/history_panel_test.cpp
        tests/history_test.cpp
        tests/icons_test.cpp
        tests/indexed_test.cpp
        tests/indexing_test.cpp
        tests/mode_change_test.cpp
        tests/arc_test.cpp
        tests/background_effect_test.cpp
        tests/layers_panel_test.cpp
        tests/levels_test.cpp
        tests/main_frame_test.cpp
        tests/openraster_test.cpp
        tests/palettes_test.cpp
        tests/pieces_test.cpp
        tests/plugin_api_test.cpp
        tests/plugin_trial_test.cpp
        tests/plugins_test.cpp
        tests/recent_files_test.cpp
        tests/render_test.cpp
        tests/selection_test.cpp
        tests/shapes_test.cpp
        tests/stroke_test.cpp
        tests/test_card_test.cpp
        tests/text_test.cpp
        tests/tool_panel_test.cpp
        tests/tools_test.cpp
        tests/transform_test.cpp
        tests/translation_test.cpp
        tests/window_place_test.cpp)
    target_link_libraries(oui_paint_tests PRIVATE oui_paint_core GTest::gtest_main)
    target_compile_definitions(oui_paint_tests PRIVATE OUI_PAINT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    gtest_discover_tests(oui_paint_tests)
endif()
