cmake_minimum_required(VERSION 3.21)
project(Liq2048 VERSION 1.2.0 LANGUAGES C CXX
        DESCRIPTION "Physics-based blob merging puzzle in C++17 and SDL2"
        HOMEPAGE_URL "https://github.com/sobhanmohammadi-dev/Liq2048")

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
option(LIQ_FETCH_DEPS "Download and build SDL2 and SDL2_ttf from source" OFF)
option(LIQ_BUILD_TESTS "Build the unit tests" ON)
option(LIQ_BUILD_TOOLS "Build developer tools (liq_audiogen: regenerates the OGG audio)" OFF)
option(LIQ_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (used by CI)" OFF)

# ---------------------------------------------------------------------------
# Dependencies
#   Desktop : installed SDL2 / SDL2_ttf via find_package
#   Mobile  : downloaded + built from source with FetchContent (forced ON)
# ---------------------------------------------------------------------------
if(ANDROID OR IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(LIQ_FETCH_DEPS ON CACHE BOOL "" FORCE)
endif()

if(LIQ_FETCH_DEPS)
  include(FetchContent)
  if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
  endif()
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.30.9)
  FetchContent_Declare(SDL2_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git GIT_TAG release-2.22.0)
  FetchContent_MakeAvailable(SDL2 SDL2_ttf)
else()
  find_package(SDL2 REQUIRED)
  find_package(SDL2_ttf REQUIRED)
endif()

if(TARGET SDL2::SDL2)
  set(LIQ_SDL_TARGET SDL2::SDL2)
else()
  set(LIQ_SDL_TARGET SDL2::SDL2-static)
endif()

# ---------------------------------------------------------------------------
# Game core library (everything except main.cpp) so tests/tools can reuse it
# ---------------------------------------------------------------------------
file(GLOB_RECURSE LIQ_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER LIQ_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")

# stb_vorbis (public domain OGG Vorbis decoder), compiled as plain C without warnings.
add_library(stb_vorbis STATIC third_party/stb_vorbis/stb_vorbis.c)
target_include_directories(stb_vorbis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb_vorbis)
if(NOT MSVC)
  target_compile_options(stb_vorbis PRIVATE -w)
endif()
if(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries(stb_vorbis PUBLIC m)
endif()

add_library(liq_core STATIC ${LIQ_SOURCES})
target_include_directories(liq_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(liq_core PUBLIC stb_vorbis)
target_link_libraries(liq_core PUBLIC ${LIQ_SDL_TARGET} SDL2_ttf::SDL2_ttf)
target_compile_definitions(liq_core PUBLIC LIQ_VERSION="${PROJECT_VERSION}")
if(MSVC)
  target_compile_options(liq_core PRIVATE /W4 /utf-8)
  if(LIQ_WARNINGS_AS_ERRORS)
    target_compile_options(liq_core PRIVATE /WX)
  endif()
else()
  target_compile_options(liq_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
  if(LIQ_WARNINGS_AS_ERRORS)
    target_compile_options(liq_core PRIVATE -Werror)
  endif()
endif()

# ---------------------------------------------------------------------------
# Executable (Android needs a shared library called "main")
# ---------------------------------------------------------------------------
if(ANDROID)
  add_library(liq2048 SHARED src/main.cpp)
  set_target_properties(liq2048 PROPERTIES OUTPUT_NAME "main")
else()
  add_executable(liq2048 src/main.cpp)
endif()
target_link_libraries(liq2048 PRIVATE liq_core)
if(TARGET SDL2::SDL2main)
  target_link_libraries(liq2048 PRIVATE SDL2::SDL2main)
endif()
if(WIN32)
  set_target_properties(liq2048 PROPERTIES WIN32_EXECUTABLE ON)
endif()
if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # iOS app bundle: Info.plist, app icon and the assets/ folder copied into the bundle.
  file(GLOB_RECURSE LIQ_IOS_ASSETS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*)
  list(FILTER LIQ_IOS_ASSETS EXCLUDE REGEX "\\.gitkeep$")
  foreach(f ${LIQ_IOS_ASSETS})
    file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${f})
    get_filename_component(dir ${rel} DIRECTORY)
    set_source_files_properties(${f} PROPERTIES MACOSX_PACKAGE_LOCATION ${dir})
  endforeach()
  set(LIQ_ICONS ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets)
  set_source_files_properties(${LIQ_ICONS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  target_sources(liq2048 PRIVATE ${LIQ_IOS_ASSETS} ${LIQ_ICONS})
  set_target_properties(liq2048 PROPERTIES
    MACOSX_BUNDLE ON
    OUTPUT_NAME "Liq2048"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sobhanmohammadi.liq2048"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "dev.sobhanmohammadi.liq2048"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
endif()

# Make assets/ available next to the binary on desktop builds.
if(NOT ANDROID AND NOT IOS)
  add_custom_command(TARGET liq2048 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:liq2048>/assets)
endif()

# ---------------------------------------------------------------------------
# Developer tools (not shipped): audio generator, see tools/audiogen/build_audio.py
# ---------------------------------------------------------------------------
if(LIQ_BUILD_TOOLS AND NOT ANDROID AND NOT IOS)
  add_executable(liq_audiogen tools/audiogen/main.cpp tools/audiogen/Synth.cpp)
endif()

# ---------------------------------------------------------------------------
# Tests (ctest)
# ---------------------------------------------------------------------------
if(LIQ_BUILD_TESTS AND NOT ANDROID AND NOT IOS AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  enable_testing()
  add_executable(liq_tests tests/test_main.cpp)
  target_link_libraries(liq_tests PRIVATE liq_core)
  add_test(NAME liq_tests COMMAND liq_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ---------------------------------------------------------------------------
# Install + packaging (cpack -G ZIP / TGZ): game folder with assets next to the binary
# ---------------------------------------------------------------------------
if(NOT ANDROID AND NOT IOS AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  install(TARGETS liq2048 RUNTIME DESTINATION .)
  install(DIRECTORY assets DESTINATION . PATTERN ".gitkeep" EXCLUDE)
  install(FILES README.md LICENSE CHANGELOG.md DESTINATION .)
  set(CPACK_PACKAGE_NAME "Liq2048")
  set(CPACK_PACKAGE_VENDOR "Sobhan Mohammadi")
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
  set(CPACK_GENERATOR "ZIP;TGZ")
  include(CPack)
endif()
