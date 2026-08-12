cmake_minimum_required(VERSION 3.20)

project(keepsake
    VERSION 0.1.0
    DESCRIPTION "A text-based RPG where your AT Protocol DID is the save file"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Phase 1 (current): local-only, no third-party dependencies.
#
# Phase 2 will add an optional wolfram-backed sync/ implementation, the same
# way bluesky-dadaist pulls in the SDK from a sibling checkout:
#
#   set(WOLFRAM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../wolfram" ...)
#   add_subdirectory("${WOLFRAM_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/wolfram")
#   target_link_libraries(keepsake PRIVATE wolfram wolfram_cpp)
#
# Not wired up yet — see AGENTS.md "Current reality and risks".

add_executable(keepsake
    src/main.cpp
    src/util/xdg.cpp
    src/identity/identity.cpp
    src/save/json.cpp
    src/save/save.cpp
    src/sync/local_record_store.cpp
    src/world/world.cpp
    src/entity/character.cpp
    src/combat/combat.cpp
    src/dialogue/dialogue.cpp
    src/quest/quest.cpp
    src/ui/terminal.cpp
)

target_include_directories(keepsake PRIVATE src)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(keepsake PRIVATE -Wall -Wextra)
endif()
