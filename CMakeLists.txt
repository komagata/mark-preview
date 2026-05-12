cmake_minimum_required(VERSION 3.20)
project(mark-preview
    VERSION 0.1.0
    DESCRIPTION "Simple Markdown viewer for Omarchy / Linux / macOS / Windows"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# --- wxWidgets ---------------------------------------------------------------
find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base webview)
include(${wxWidgets_USE_FILE})

# --- WebKit2GTK (Linux only — needed for allow_file_access workaround) ------
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT2GTK REQUIRED IMPORTED_TARGET webkit2gtk-4.1)
endif()

# --- cmark-gfm ---------------------------------------------------------------
# Arch ships pkg-config for the core lib but not for extensions.
# Probe both via find_path/find_library so it works on Homebrew/vcpkg too.
find_path(CMARK_GFM_INCLUDE_DIR cmark-gfm.h)
find_library(CMARK_GFM_LIBRARY NAMES cmark-gfm)
find_library(CMARK_GFM_EXTENSIONS_LIBRARY NAMES cmark-gfm-extensions)
if(NOT CMARK_GFM_INCLUDE_DIR OR NOT CMARK_GFM_LIBRARY OR NOT CMARK_GFM_EXTENSIONS_LIBRARY)
    message(FATAL_ERROR "cmark-gfm not found. Install 'cmark-gfm' (Arch), or 'cmark-gfm' (Homebrew), or 'cmark-gfm' (vcpkg).")
endif()
message(STATUS "cmark-gfm headers: ${CMARK_GFM_INCLUDE_DIR}")
message(STATUS "cmark-gfm lib:     ${CMARK_GFM_LIBRARY}")
message(STATUS "cmark-gfm ext lib: ${CMARK_GFM_EXTENSIONS_LIBRARY}")

# --- Embed assets as C++ headers ---------------------------------------------
# Run scripts/fetch-assets.sh BEFORE configuring CMake so assets/ is populated.
include(EmbedAssets)
set(GENERATED_ASSETS_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_ASSETS_DIR})

set(ASSET_FILES
    assets/template.html
    assets/github-light.css
    assets/github-dark.css
    assets/app.js
    assets/katex/katex.min.css
    assets/katex/katex.min.js
    assets/katex/auto-render.min.js
    assets/mermaid/mermaid.min.js
    assets/highlight/highlight.min.js
    assets/highlight/github.min.css
    assets/highlight/github-dark.min.css
)

# Glob KaTeX fonts (variable number of files).
file(GLOB KATEX_FONT_FILES RELATIVE ${CMAKE_SOURCE_DIR}
     assets/katex/fonts/*.woff2)
list(SORT KATEX_FONT_FILES)

set(ALL_ASSETS ${ASSET_FILES} ${KATEX_FONT_FILES})

set(EMBEDDED_ASSET_HEADERS)
set(EMBEDDED_INCLUDES "")
set(EMBEDDED_TABLE "")
foreach(asset ${ALL_ASSETS})
    embed_asset(${CMAKE_SOURCE_DIR}/${asset} ${GENERATED_ASSETS_DIR} EMBEDDED_ASSET_HEADERS)

    file(RELATIVE_PATH ASSET_REL ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/${asset})
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" ASSET_SYM ${asset})
    string(REGEX REPLACE "^_+" "" ASSET_SYM ${ASSET_SYM})
    string(APPEND EMBEDDED_INCLUDES "#include \"embedded/${ASSET_SYM}.h\"\n")
    string(APPEND EMBEDDED_TABLE "    { \"${ASSET_REL}\", mp::embedded::${ASSET_SYM}, mp::embedded::${ASSET_SYM}_len },\n")
endforeach()

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/EmbeddedAssetsTable.h.in
    ${GENERATED_ASSETS_DIR}/EmbeddedAssetsTable.h
    @ONLY
)

add_custom_target(generate_assets DEPENDS ${EMBEDDED_ASSET_HEADERS})

# --- Executable --------------------------------------------------------------
add_executable(mark-preview
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainFrame.cpp
    src/Renderer.cpp
    src/Watcher.cpp
    src/Theme.cpp
    src/Resources.cpp
)

if(APPLE)
    set_target_properties(mark-preview PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME           "mark-preview"
        MACOSX_BUNDLE_BUNDLE_VERSION        "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING  "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER        "org.komagata.mark-preview"
        MACOSX_BUNDLE_COPYRIGHT             "MIT"
    )
endif()
add_dependencies(mark-preview generate_assets)

target_include_directories(mark-preview PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GENERATED_ASSETS_DIR}
    ${CMARK_GFM_INCLUDE_DIR}
)

target_link_libraries(mark-preview PRIVATE
    ${wxWidgets_LIBRARIES}
    ${CMARK_GFM_LIBRARY}
    ${CMARK_GFM_EXTENSIONS_LIBRARY}
)
if(UNIX AND NOT APPLE)
    target_link_libraries(mark-preview PRIVATE PkgConfig::WEBKIT2GTK)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(mark-preview PRIVATE stdc++fs)
endif()

include(GNUInstallDirs)

install(TARGETS mark-preview
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE  DESTINATION .)

if(UNIX AND NOT APPLE)
    install(FILES packaging/mark-preview.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/mark-preview.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
    install(FILES LICENSE
            DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/mark-preview)
endif()

# --- mp-render: console-only renderer used by tests --------------------------
add_executable(mp-render
    src/render_tool.cpp
    src/Renderer.cpp
    src/Resources.cpp
    src/Theme.cpp
)
add_dependencies(mp-render generate_assets)
target_include_directories(mp-render PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GENERATED_ASSETS_DIR}
    ${CMARK_GFM_INCLUDE_DIR}
)
target_link_libraries(mp-render PRIVATE
    ${wxWidgets_LIBRARIES}
    ${CMARK_GFM_LIBRARY}
    ${CMARK_GFM_EXTENSIONS_LIBRARY}
)
