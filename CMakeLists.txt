cmake_minimum_required(VERSION 3.16)
project(Brumeval LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# --- SDL2 : téléchargée et compilée automatiquement au premier "configure" ---
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL2)

# --- Le jeu ---
file(GLOB BRUMEVAL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
# Console visible : utile pour lire les messages du mode test (--test)
add_executable(brumeval ${BRUMEVAL_SOURCES})
target_include_directories(brumeval PRIVATE src)

if(TARGET SDL2::SDL2main)
  target_link_libraries(brumeval PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(brumeval PRIVATE SDL2::SDL2-static)

if(MSVC)
  # /utf-8 : indispensable pour les accents dans les textes du jeu
  target_compile_options(brumeval PRIVATE /utf-8 /W3)
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT brumeval)
else()
  target_compile_options(brumeval PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
