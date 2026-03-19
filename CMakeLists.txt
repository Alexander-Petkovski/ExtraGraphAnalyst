cmake_minimum_required(VERSION 3.20)
project(ExtraGraphAnalyst LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Python ────────────────────────────────────────────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Development)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ChartWindow.cpp
    src/PythonBridge.cpp
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(ExtraGraphAnalyst WIN32 ${SOURCES})

# ── Includes ──────────────────────────────────────────────────────────────────
target_include_directories(ExtraGraphAnalyst PRIVATE
    src
    ${Python3_INCLUDE_DIRS}
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
