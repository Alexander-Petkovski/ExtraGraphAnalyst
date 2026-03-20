cmake_minimum_required(VERSION 3.20)
project(ExtraGraphAnalyst LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Python ────────────────────────────────────────────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Development)

# ── Generate app.rc with absolute icon path ──────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    @ONLY
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ChartWindow.cpp
    src/PythonBridge.cpp
    src/WorkspaceFile.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(ExtraGraphAnalyst WIN32 ${SOURCES})

# ── Includes (CXX only — keeps windres from choking on paths with spaces) ────
target_include_directories(ExtraGraphAnalyst PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<$<COMPILE_LANGUAGE:CXX>:${Python3_INCLUDE_DIRS}>
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(ExtraGraphAnalyst PRIVATE
    ${Python3_LIBRARIES}
    gdiplus
    gdi32
    user32
    comctl32
    comdlg32
    shell32
    advapi32
    shlwapi
    uxtheme
    dwmapi
)

# ── Definitions ───────────────────────────────────────────────────────────────
target_compile_definitions(ExtraGraphAnalyst PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
