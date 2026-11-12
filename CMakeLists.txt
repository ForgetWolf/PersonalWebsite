cmake_minimum_required(VERSION 3.21)
project(static_packager LANGUAGES C CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LibXml2 REQUIRED)
add_executable(pack-home builder/pack-home.cpp builder/home-welcome.cpp builder/home-terminal.cpp)
target_compile_features(pack-home PRIVATE cxx_std_17)
target_link_libraries(pack-home PRIVATE LibXml2::LibXml2)
if(NOT MSVC)
    target_link_libraries(pack-home PRIVATE m)
endif()

include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMARK_SHARED OFF)
set(CMARK_STATIC ON)
set(CMARK_TESTS OFF)
set(CMARK_ARCHIVE "https://codeload.github.com/github/cmark-gfm/tar.gz/refs/tags/0.29.0.gfm.13"
    CACHE STRING "URL or local path to the cmark-gfm release archive")
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(cmark_gfm
    URL "${CMARK_ARCHIVE}"
    URL_HASH SHA256=5abc61798ebd9de5660bc076443c07abad2b8d15dbc11094a3a79644b8ad243a
)
FetchContent_MakeAvailable(cmark_gfm)

add_executable(pack-page builder/pack-page.cpp)
target_compile_features(pack-page PRIVATE cxx_std_17)
target_compile_definitions(pack-page PRIVATE CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
target_include_directories(pack-page PRIVATE
    "${cmark_gfm_SOURCE_DIR}/src" "${cmark_gfm_BINARY_DIR}/src"
    "${cmark_gfm_SOURCE_DIR}/extensions" "${cmark_gfm_BINARY_DIR}/extensions"
)
target_link_libraries(pack-page PRIVATE
    libcmark-gfm-extensions_static libcmark-gfm_static LibXml2::LibXml2)

add_custom_target(package
    COMMAND "${CMAKE_COMMAND}"
        "-DSITE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DDIST=${CMAKE_CURRENT_SOURCE_DIR}/dist"
        "-DHOME_STAGE=${CMAKE_CURRENT_BINARY_DIR}/index.html.tmp"
        "-DPACK_HOME=$<TARGET_FILE:pack-home>"
        "-DPACK_PAGE=$<TARGET_FILE:pack-page>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/builder/package.cmake"
    DEPENDS pack-home pack-page
    VERBATIM
)

add_executable(build-gost EXCLUDE_FROM_ALL builder/build-gost.cpp)
target_compile_features(build-gost PRIVATE cxx_std_17)
if(NOT MSVC)
    target_link_libraries(build-gost PRIVATE m)
endif()

set(FONTFORGE_EXECUTABLE "" CACHE FILEPATH "Optional FontForge command for font regeneration")
add_custom_target(gost-fonts
    COMMAND "${CMAKE_COMMAND}"
        "-DFONTFORGE_EXECUTABLE=${FONTFORGE_EXECUTABLE}"
        "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/font-rebuild"
        "-DGENERATOR=$<TARGET_FILE:build-gost>"
        "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/GOST-font.coffee"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/builder/gost-fonts.cmake"
    DEPENDS build-gost
    VERBATIM
)
