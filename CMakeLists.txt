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

# Phase 2: wolfram-backed identity/sync, pulled in from a sibling checkout
# the same way bluesky-dadaist does. Optional so Phase 1's local-only game
# still builds standalone (KEEPSAKE_WITH_WOLFRAM=OFF) if wolfram isn't
# checked out next to this repo.
option(KEEPSAKE_WITH_WOLFRAM "Build the wolfram-backed OAuth/sync layer" ON)

set(KEEPSAKE_SOURCES
    src/main.cpp
    src/util/xdg.cpp
    src/util/time.cpp
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

# Unit tests for the pure-logic modules — quest/, combat/, save/, dialogue/,
# identity/ never include anything from sync/ (see AGENTS.md's module
# boundary rules), so these need no wolfram dependency and no network
# access, and are always registered regardless of KEEPSAKE_WITH_WOLFRAM.
enable_testing()

function(keepsake_add_test name)
    add_executable(${name} test/${name}.cpp ${ARGN})
    target_include_directories(${name} PRIVATE src)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

keepsake_add_test(quest_test src/quest/quest.cpp)
keepsake_add_test(combat_test src/combat/combat.cpp src/entity/character.cpp)
keepsake_add_test(json_test src/save/json.cpp)
keepsake_add_test(save_test
    src/save/json.cpp src/save/save.cpp src/util/xdg.cpp
    src/entity/character.cpp src/quest/quest.cpp)
keepsake_add_test(dialogue_test
    src/dialogue/dialogue.cpp src/entity/character.cpp src/quest/quest.cpp)
keepsake_add_test(identity_test
    src/identity/identity.cpp src/save/json.cpp src/util/xdg.cpp)
keepsake_add_test(world_test src/world/world.cpp src/quest/quest.cpp)

if(KEEPSAKE_WITH_WOLFRAM)
    # Build only what the OAuth/sync layer needs — no server, no bundled
    # tests/examples, and skip the optional SQLite-backed session store
    # (WOLFRAM_BUILD_STORE): Phase 2 serializes wf_oauth_session_state to a
    # plain JSON file itself (see src/oauth/), which wolfram's own docs
    # confirm is a sound simplification for a first pass.
    # WOLFRAM_BUILD_CPP stays off: the RAII wrapper's generated_owners.hpp
    # covers draft/temp/bookmark typed wrappers this app doesn't use, and
    # pulls in its own codegen step. Phase 2 writes its own small local RAII
    # guards in src/oauth/oauth_flow.cpp instead of taking that dependency.
    set(WOLFRAM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WOLFRAM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WOLFRAM_BUILD_SERVER OFF CACHE BOOL "" FORCE)

    # A local checkout next to this repo (as bluesky-dadaist uses) is tried
    # first — fastest inner loop, and lets a wolfram working copy be
    # exercised without pushing it anywhere first. It is never required
    # though: with nothing at WOLFRAM_SOURCE_DIR, configure fetches
    # wolfram's latest tagged GitHub release automatically via
    # FetchContent, so `cmake -S . -B build` works from a bare checkout of
    # this repo alone.
    set(WOLFRAM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../wolfram" CACHE PATH
        "Path to a local wolfram AT Protocol SDK checkout, preferred over fetching one if present")

    if(EXISTS "${WOLFRAM_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "keepsake: using local wolfram checkout at ${WOLFRAM_SOURCE_DIR}")
        add_subdirectory("${WOLFRAM_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/wolfram")
    else()
        include(FetchContent)
        find_package(Git REQUIRED)

        # Resolve the latest vMAJOR.MINOR.PATCH tag via `git ls-remote`
        # rather than pinning one in this file, so keepsake tracks
        # wolfram's releases without needing an update here each time.
        # Falls back to `main` (with a warning) if no tags are reachable —
        # e.g. offline, or GitHub unreachable.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} ls-remote --tags --refs
                    https://github.com/ewanc26/wolfram.git
            OUTPUT_VARIABLE WOLFRAM_REMOTE_TAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE WOLFRAM_LS_REMOTE_RESULT)

        set(WOLFRAM_GIT_TAG "main")
        if(WOLFRAM_LS_REMOTE_RESULT EQUAL 0)
            string(REGEX MATCHALL "refs/tags/v[0-9]+\\.[0-9]+\\.[0-9]+"
                          WOLFRAM_TAG_REFS "${WOLFRAM_REMOTE_TAGS}")
            set(WOLFRAM_TAG_LIST)
            foreach(ref IN LISTS WOLFRAM_TAG_REFS)
                string(REPLACE "refs/tags/" "" tag "${ref}")
                list(APPEND WOLFRAM_TAG_LIST "${tag}")
            endforeach()
            if(WOLFRAM_TAG_LIST)
                list(REMOVE_DUPLICATES WOLFRAM_TAG_LIST)
                list(SORT WOLFRAM_TAG_LIST COMPARE NATURAL)
                list(GET WOLFRAM_TAG_LIST -1 WOLFRAM_GIT_TAG)
            endif()
        endif()

        if(WOLFRAM_GIT_TAG STREQUAL "main")
            message(WARNING
                "keepsake: couldn't determine wolfram's latest release tag "
                "(offline, or github.com unreachable) — fetching main instead")
        else()
            message(STATUS "keepsake: fetching wolfram ${WOLFRAM_GIT_TAG} from GitHub")
        endif()

        FetchContent_Declare(
            wolfram
            GIT_REPOSITORY https://github.com/ewanc26/wolfram.git
            GIT_TAG ${WOLFRAM_GIT_TAG}
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(wolfram)
    endif()

    list(APPEND KEEPSAKE_SOURCES
        src/oauth/url_encode.cpp
        src/oauth/profile_lookup.cpp
        src/oauth/loopback_listener.cpp
        src/oauth/oauth_flow.cpp
        src/sync/wolfram_record_store.cpp
        src/sync/firehose_watch.cpp
    )

    # The one part of the firehose reader that couldn't be exercised
    # against live data during development (see AGENTS.md — wss://
    # connections were blocked in that environment) gets a synthetic
    # round-trip test instead: encode a real record, wrap it in a real
    # CAR block, decode it back with the exact function the live path
    # calls. No network access needed.
    add_executable(firehose_decode_test
        test/firehose_decode_test.cpp
        src/sync/firehose_watch.cpp
    )
    target_include_directories(firehose_decode_test PRIVATE src)
    target_link_libraries(firehose_decode_test PRIVATE wolfram)
    add_test(NAME firehose_decode_test COMMAND firehose_decode_test)
endif()

add_executable(keepsake ${KEEPSAKE_SOURCES})

target_include_directories(keepsake PRIVATE src)

if(KEEPSAKE_WITH_WOLFRAM)
    find_package(Threads REQUIRED)
    target_link_libraries(keepsake PRIVATE wolfram Threads::Threads)
    target_compile_definitions(keepsake PRIVATE KEEPSAKE_WITH_WOLFRAM=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(keepsake PRIVATE -Wall -Wextra)
endif()
