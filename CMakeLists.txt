cmake_minimum_required(VERSION 3.16)
project(zc95-lua-builder VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ZC95_BUILD_TESTS "Build the Qt Test round-trip suite" ON)

find_package(Qt6 6.2 COMPONENTS Widgets REQUIRED)
if(ZC95_BUILD_TESTS)
    find_package(Qt6 6.2 COMPONENTS Test QUIET)
endif()

# ---------------------------------------------------------------------------
# Lua 5.4 — fetched and built as a static library (no system dep required).
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    lua
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
)
FetchContent_MakeAvailable(lua)

# Lua's official tarball ships only a Makefile; build the core sources as a
# static library ourselves.
file(GLOB LUA_CORE_SOURCES
    "${lua_SOURCE_DIR}/src/lapi.c"
    "${lua_SOURCE_DIR}/src/lcode.c"
    "${lua_SOURCE_DIR}/src/lctype.c"
    "${lua_SOURCE_DIR}/src/ldebug.c"
    "${lua_SOURCE_DIR}/src/ldo.c"
    "${lua_SOURCE_DIR}/src/ldump.c"
    "${lua_SOURCE_DIR}/src/lfunc.c"
    "${lua_SOURCE_DIR}/src/lgc.c"
    "${lua_SOURCE_DIR}/src/llex.c"
    "${lua_SOURCE_DIR}/src/lmem.c"
    "${lua_SOURCE_DIR}/src/lobject.c"
    "${lua_SOURCE_DIR}/src/lopcodes.c"
    "${lua_SOURCE_DIR}/src/lparser.c"
    "${lua_SOURCE_DIR}/src/lstate.c"
    "${lua_SOURCE_DIR}/src/lstring.c"
    "${lua_SOURCE_DIR}/src/ltable.c"
    "${lua_SOURCE_DIR}/src/ltm.c"
    "${lua_SOURCE_DIR}/src/lundump.c"
    "${lua_SOURCE_DIR}/src/lvm.c"
    "${lua_SOURCE_DIR}/src/lzio.c"
    "${lua_SOURCE_DIR}/src/lauxlib.c"
    "${lua_SOURCE_DIR}/src/lbaselib.c"
    "${lua_SOURCE_DIR}/src/lcorolib.c"
    "${lua_SOURCE_DIR}/src/ldblib.c"
    "${lua_SOURCE_DIR}/src/liolib.c"
    "${lua_SOURCE_DIR}/src/lmathlib.c"
    "${lua_SOURCE_DIR}/src/loadlib.c"
    "${lua_SOURCE_DIR}/src/loslib.c"
    "${lua_SOURCE_DIR}/src/lstrlib.c"
    "${lua_SOURCE_DIR}/src/ltablib.c"
    "${lua_SOURCE_DIR}/src/lutf8lib.c"
    "${lua_SOURCE_DIR}/src/linit.c"
)
add_library(lua_static STATIC ${LUA_CORE_SOURCES})
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}/src")
if(UNIX AND NOT APPLE)
    target_compile_definitions(lua_static PUBLIC LUA_USE_LINUX)
elseif(APPLE)
    target_compile_definitions(lua_static PUBLIC LUA_USE_MACOSX)
endif()
set_target_properties(lua_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Source groups
# ---------------------------------------------------------------------------
set(MODEL_SOURCES
    src/model/ScriptConfig.h
    src/model/ScriptConfig.cpp
    src/model/MenuItem.h
    src/model/MenuItem.cpp
)

set(CODEGEN_SOURCES
    src/codegen/LuaGenerator.h
    src/codegen/LuaGenerator.cpp
    src/codegen/LuaParser.h
    src/codegen/LuaParser.cpp
    src/codegen/Linter.h
    src/codegen/Linter.cpp
    src/codegen/ApiDoc.h
    src/codegen/ApiDoc.cpp
)

set(SIM_SOURCES
    src/sim/ChannelEvent.h
    src/sim/LuaRuntime.h
    src/sim/LuaRuntime.cpp
)

set(UI_SOURCES
    src/highlight/LuaHighlighter.h
    src/highlight/LuaHighlighter.cpp
    src/ui/ConfigPanel.h
    src/ui/ConfigPanel.cpp
    src/ui/MenuItemsPanel.h
    src/ui/MenuItemsPanel.cpp
    src/ui/MenuItemDialog.h
    src/ui/MenuItemDialog.cpp
    src/ui/FunctionsPanel.h
    src/ui/FunctionsPanel.cpp
    src/ui/SnippetsPanel.h
    src/ui/SnippetsPanel.cpp
    src/ui/LuaEditor.h
    src/ui/LuaEditor.cpp
    src/ui/FindReplaceBar.h
    src/ui/FindReplaceBar.cpp
    src/ui/IssuesPanel.h
    src/ui/IssuesPanel.cpp
    src/ui/ApiDocPanel.h
    src/ui/ApiDocPanel.cpp
    src/ui/LcdPreviewPanel.h
    src/ui/LcdPreviewPanel.cpp
    src/ui/TimelineWidget.h
    src/ui/TimelineWidget.cpp
    src/ui/SimulatorPanel.h
    src/ui/SimulatorPanel.cpp
)

set(APP_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
qt_add_executable(zc95-lua-builder
    ${APP_SOURCES}
    ${MODEL_SOURCES}
    ${CODEGEN_SOURCES}
    ${SIM_SOURCES}
    ${UI_SOURCES}
    resources/presets.qrc
)
target_include_directories(zc95-lua-builder PRIVATE src)
target_link_libraries(zc95-lua-builder PRIVATE Qt6::Widgets lua_static)

set_target_properties(zc95-lua-builder PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# ---------------------------------------------------------------------------
# Tests (round-trip generator/parser/linter)
# ---------------------------------------------------------------------------
if(ZC95_BUILD_TESTS AND Qt6Test_FOUND)
    enable_testing()
    qt_add_executable(test_roundtrip
        tests/test_roundtrip.cpp
        ${MODEL_SOURCES}
        ${CODEGEN_SOURCES}
        resources/presets.qrc
    )
    target_include_directories(test_roundtrip PRIVATE src)
    target_link_libraries(test_roundtrip PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME test_roundtrip COMMAND test_roundtrip)
endif()
