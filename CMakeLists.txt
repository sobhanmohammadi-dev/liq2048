cmake_minimum_required(VERSION 3.21)

project(Liq2048
    VERSION 1.9.4
    LANGUAGES C CXX
    DESCRIPTION "Physics-based blob merging puzzle in C++17 and SDL2"
    HOMEPAGE_URL "https://github.com/sobhanmohammadi-dev/Liq2048"
)

# ---------------------------------------------------------------------------
# C++ configuration
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(LIQ_FETCH_DEPS
    "Download and build SDL2 and SDL2_ttf from source"
    OFF
)

option(LIQ_BUILD_TESTS
    "Build the unit tests"
    ON
)

option(LIQ_BUILD_TOOLS
    "Build developer tools (liq_audiogen, liq_puzzlegen and liq_bench)"
    OFF
)

option(LIQ_LTO
    "Link-time optimisation for Release builds (smaller and usually faster; ignored when unsupported)"
    ON
)

option(LIQ_STRIP
    "Strip symbols from the Release executable"
    ON
)

option(LIQ_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors"
    OFF
)

# ---------------------------------------------------------------------------
# Dependencies
#
# Desktop:
#   Use installed SDL2 / SDL2_ttf unless LIQ_FETCH_DEPS is enabled.
#
# Android / iOS:
#   Build dependencies from source using FetchContent.
# ---------------------------------------------------------------------------

if(ANDROID OR IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(LIQ_FETCH_DEPS ON CACHE BOOL "" FORCE)
endif()

if(LIQ_FETCH_DEPS)

    include(FetchContent)

    # -----------------------------------------------------------------------
    # SDL2 library configuration
    # -----------------------------------------------------------------------

    if(ANDROID)

        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)

        # SDL2_ttf must use the shared SDL2 target on Android.
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

    else()

        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)

        # SDL2_ttf uses the static SDL2 target on desktop and iOS.
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    endif()

    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

    # -----------------------------------------------------------------------
    # SDL2_ttf configuration
    # -----------------------------------------------------------------------

    set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)

    # -----------------------------------------------------------------------
    # Fetch SDL2
    # -----------------------------------------------------------------------

    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.9
        GIT_SHALLOW TRUE
    )

    # -----------------------------------------------------------------------
    # Fetch SDL2_ttf and its FreeType submodule
    # -----------------------------------------------------------------------

    FetchContent_Declare(
        SDL2_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-2.22.0
        GIT_SHALLOW TRUE
        GIT_SUBMODULES external/freetype
    )

    FetchContent_MakeAvailable(SDL2 SDL2_ttf)

else()

    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)

endif()

# ---------------------------------------------------------------------------
# SDL2 target selection
# ---------------------------------------------------------------------------

if(TARGET SDL2::SDL2)

    set(LIQ_SDL_TARGET SDL2::SDL2)

elseif(TARGET SDL2::SDL2-static)

    set(LIQ_SDL_TARGET SDL2::SDL2-static)

elseif(TARGET SDL2)

    set(LIQ_SDL_TARGET SDL2)

elseif(TARGET SDL2-static)

    set(LIQ_SDL_TARGET SDL2-static)

else()

    message(FATAL_ERROR
        "No compatible SDL2 target found. "
        "Expected SDL2::SDL2 or SDL2::SDL2-static."
    )

endif()

# ---------------------------------------------------------------------------
# SDL2_ttf target selection
# ---------------------------------------------------------------------------

if(TARGET SDL2_ttf::SDL2_ttf)

    set(LIQ_SDL_TTF_TARGET SDL2_ttf::SDL2_ttf)

elseif(TARGET SDL2_ttf::SDL2_ttf-static)

    set(LIQ_SDL_TTF_TARGET SDL2_ttf::SDL2_ttf-static)

elseif(TARGET SDL2_ttf)

    set(LIQ_SDL_TTF_TARGET SDL2_ttf)

elseif(TARGET SDL2::ttf)

    set(LIQ_SDL_TTF_TARGET SDL2::ttf)

else()

    message(FATAL_ERROR
        "No compatible SDL2_ttf target found."
    )

endif()

message(STATUS "Liq2048 SDL2 target: ${LIQ_SDL_TARGET}")
message(STATUS "Liq2048 SDL2_ttf target: ${LIQ_SDL_TTF_TARGET}")

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

file(
    GLOB_RECURSE LIQ_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(
    FILTER LIQ_SOURCES
    EXCLUDE REGEX ".*/src/main\\.cpp$"
)

# ---------------------------------------------------------------------------
# stb_vorbis
#
# Public-domain OGG Vorbis decoder.
# Compiled as C without project warning flags.
# ---------------------------------------------------------------------------

add_library(stb_vorbis STATIC
    third_party/stb_vorbis/stb_vorbis.c
)

target_include_directories(stb_vorbis
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb_vorbis"
)

if(NOT MSVC)
    target_compile_options(stb_vorbis PRIVATE -w)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(stb_vorbis PUBLIC m)
endif()

# ---------------------------------------------------------------------------
# Core library
#
# Contains all game code except main.cpp.
# SDL2 and SDL2_ttf must be PUBLIC dependencies because liq_core is static.
# ---------------------------------------------------------------------------

add_library(liq_core STATIC
    ${LIQ_SOURCES}
)

target_include_directories(liq_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(liq_core
    PUBLIC
        stb_vorbis
        ${LIQ_SDL_TARGET}
        ${LIQ_SDL_TTF_TARGET}
)
if(WIN32)
    target_link_libraries(liq_core PUBLIC ws2_32)  # LAN Battle sockets
endif()

target_compile_definitions(liq_core
    PUBLIC
        LIQ_VERSION="${PROJECT_VERSION}"
)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------

if(MSVC)

    target_compile_options(liq_core PRIVATE
        /W4
        /utf-8
        /wd4244  # int/float narrowing: checked by clang and gcc -Wall -Wextra instead
        /wd4267
    )
    target_compile_definitions(liq_core PRIVATE _CRT_SECURE_NO_WARNINGS)

    if(LIQ_WARNINGS_AS_ERRORS)
        target_compile_options(liq_core PRIVATE /WX)
    endif()

else()

    target_compile_options(liq_core PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
    )

    if(LIQ_WARNINGS_AS_ERRORS)
        target_compile_options(liq_core PRIVATE -Werror)
    endif()

endif()

# ---------------------------------------------------------------------------
# Size and speed: dead-code elimination, optional LTO and symbol stripping
#
# Every function and data object gets its own section so the linker can drop what nothing uses;
# LTO lets the optimiser inline across translation units (the game is split into many small modules).
# None of this changes behaviour: floating-point semantics are untouched (no -ffast-math).
# ---------------------------------------------------------------------------

if(NOT MSVC)
    target_compile_options(liq_core PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        set(LIQ_GC_LINK_FLAG -Wl,-dead_strip)
    else()
        set(LIQ_GC_LINK_FLAG -Wl,--gc-sections)
    endif()
endif()

if(LIQ_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LIQ_IPO_OK OUTPUT LIQ_IPO_MSG LANGUAGES CXX)
    if(LIQ_IPO_OK)
        set_property(TARGET liq_core PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
        set(LIQ_USE_LTO ON)
    else()
        message(STATUS "LTO unavailable: ${LIQ_IPO_MSG}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Main executable
#
# Android requires a shared library named "main".
# ---------------------------------------------------------------------------

if(ANDROID)

    add_library(liq2048 SHARED
        src/main.cpp
    )

    set_target_properties(liq2048 PROPERTIES
        OUTPUT_NAME "main"
    )

else()

    add_executable(liq2048
        src/main.cpp
    )

endif()

# ---------------------------------------------------------------------------
# Main executable linking
# ---------------------------------------------------------------------------

target_link_libraries(liq2048
    PRIVATE
        liq_core
)

if(LIQ_USE_LTO)
    set_property(TARGET liq2048 PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
endif()
if(LIQ_GC_LINK_FLAG)
    target_link_options(liq2048 PRIVATE ${LIQ_GC_LINK_FLAG})
endif()
if(LIQ_STRIP AND CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT MSVC AND NOT APPLE AND NOT ANDROID)
    target_link_options(liq2048 PRIVATE -s)
endif()

# SDL2main is only needed when the platform provides it.
if(TARGET SDL2::SDL2main)
    target_link_libraries(liq2048 PRIVATE SDL2::SDL2main)
endif()

# ---------------------------------------------------------------------------
# Windows configuration
# ---------------------------------------------------------------------------

if(WIN32)

    set_target_properties(liq2048 PROPERTIES
        WIN32_EXECUTABLE ON
    )

endif()

# ---------------------------------------------------------------------------
# iOS application bundle
# ---------------------------------------------------------------------------

if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")

    file(
        GLOB_RECURSE LIQ_IOS_ASSETS
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
    )

    list(
        FILTER LIQ_IOS_ASSETS
        EXCLUDE REGEX "\\.gitkeep$"
    )

    foreach(f IN LISTS LIQ_IOS_ASSETS)

        file(
            RELATIVE_PATH rel
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${f}"
        )

        get_filename_component(dir "${rel}" DIRECTORY)

        set_source_files_properties("${f}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "${dir}"
        )

    endforeach()

    set(LIQ_ICONS
        "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets"
    )

    set_source_files_properties("${LIQ_ICONS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )

    target_sources(liq2048 PRIVATE
        ${LIQ_IOS_ASSETS}
        ${LIQ_ICONS}
    )

    set_target_properties(liq2048 PROPERTIES

        MACOSX_BUNDLE ON

        OUTPUT_NAME "Liq2048"

        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"

        MACOSX_BUNDLE_GUI_IDENTIFIER
            "dev.sobhanmohammadi.liq2048"

        MACOSX_BUNDLE_SHORT_VERSION_STRING
            "${PROJECT_VERSION}"

        MACOSX_BUNDLE_BUNDLE_VERSION
            "${PROJECT_VERSION}"

        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
            "dev.sobhanmohammadi.liq2048"

        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"

        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME
            "AppIcon"

        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"

    )

endif()

# ---------------------------------------------------------------------------
# Desktop assets
# ---------------------------------------------------------------------------

if(NOT ANDROID AND NOT IOS)

    add_custom_command(
        TARGET liq2048
        POST_BUILD

        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:liq2048>/assets"

        COMMENT "Copying game assets..."
    )

endif()

# ---------------------------------------------------------------------------
# Developer tools
# ---------------------------------------------------------------------------

if(LIQ_BUILD_TOOLS AND NOT ANDROID AND NOT IOS)

    add_executable(liq_audiogen
        tools/audiogen/main.cpp
        tools/audiogen/Synth.cpp
    )

    add_executable(liq_bench
        tools/bench/main.cpp
    )

    target_link_libraries(liq_bench
        PRIVATE
            liq_core
    )

    add_executable(liq_puzzlegen
        tools/puzzlegen/main.cpp
    )

    target_link_libraries(liq_puzzlegen
        PRIVATE
            liq_core
    )

endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(
    LIQ_BUILD_TESTS
    AND NOT ANDROID
    AND NOT IOS
    AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR
)

    enable_testing()

    add_executable(liq_tests
        tests/test_main.cpp
    )

    target_link_libraries(liq_tests
        PRIVATE
            liq_core
    )

    add_test(
        NAME liq_tests
        COMMAND liq_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

endif()

# ---------------------------------------------------------------------------
# Install and packaging
# ---------------------------------------------------------------------------

if(
    NOT ANDROID
    AND NOT IOS
    AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR
)

    install(
        TARGETS liq2048
        RUNTIME DESTINATION .
    )

    install(
        DIRECTORY assets
        DESTINATION .
        PATTERN ".gitkeep" EXCLUDE
    )

    install(
        FILES
            README.md
            LICENSE
            CHANGELOG.md
        DESTINATION .
    )

    set(CPACK_PACKAGE_NAME "Liq2048")
    set(CPACK_PACKAGE_VENDOR "Sobhan Mohammadi")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "${PROJECT_DESCRIPTION}"
    )

    set(CPACK_RESOURCE_FILE_LICENSE
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    )

    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_GENERATOR "ZIP;TGZ")

    include(CPack)

endif()
