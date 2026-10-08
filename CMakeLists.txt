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

# --- nlohmann/json : lecture et écriture des fichiers de data/ ---
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# --- Le jeu ---
file(GLOB BRUMEVAL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
# Console visible : utile pour lire les messages du mode test (--test)
add_executable(brumeval ${BRUMEVAL_SOURCES})
target_include_directories(brumeval PRIVATE src)

if(TARGET SDL2::SDL2main)
  target_link_libraries(brumeval PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(brumeval PRIVATE SDL2::SDL2-static nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(brumeval PRIVATE ws2_32)  # réseau (multijoueur)
endif()
# Dossier data/ du projet : utilisé si on ne le trouve pas à côté du programme
target_compile_definitions(brumeval PRIVATE BRUMEVAL_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

if(MSVC)
  # /utf-8 : indispensable pour les accents dans les textes du jeu
  # /wd4244 : coupe l'avertissement « conversion de 'int' en 'float' », émis des
  # centaines de fois quand on passe des coordonnées entières aux fonctions de
  # dessin (sans danger ici). GCC ne le signale pas non plus avec -Wall -Wextra.
  target_compile_options(brumeval PRIVATE /utf-8 /W3 /wd4244)
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT brumeval)
else()
  target_compile_options(brumeval PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
