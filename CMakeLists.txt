cmake_minimum_required(VERSION 3.16)
project(malwarium LANGUAGES C CXX)  # C for the vendored qrcodegen (src/core/render/qrcodegen.c)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Portable engine sources (no hardware / SDL dependency). Also pull the vendored
# C sources (Nayuki qrcodegen) so the QR encoder links into the same lib; pio
# auto-globs .c under src/core/, this mirrors that for the host/native build.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/src/core/*.cpp
     ${CMAKE_SOURCE_DIR}/src/core/*.c)
set(GENERATED_SOURCES ${CMAKE_SOURCE_DIR}/src/generated/assets.cpp)

add_library(malcore STATIC ${CORE_SOURCES} ${GENERATED_SOURCES})
target_include_directories(malcore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include)

# --- Host preview (SDL2) ----------------------------------------------------
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    # Fall back to sdl2-config (Homebrew ships it even without CMake config).
    find_program(SDL2_CONFIG sdl2-config)
    if(SDL2_CONFIG)
        execute_process(COMMAND ${SDL2_CONFIG} --cflags
                        OUTPUT_VARIABLE SDL2_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${SDL2_CONFIG} --libs
                        OUTPUT_VARIABLE SDL2_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
        separate_arguments(SDL2_CFLAGS)
    endif()
endif()

if(SDL2_FOUND OR SDL2_CONFIG)
    add_executable(malwarium_host src/platform/host/main.cpp)
    target_link_libraries(malwarium_host PRIVATE malcore)
    if(SDL2_FOUND)
        target_link_libraries(malwarium_host PRIVATE SDL2::SDL2)
    else()
        target_compile_options(malwarium_host PRIVATE ${SDL2_CFLAGS})
        target_link_libraries(malwarium_host PRIVATE ${SDL2_LIBS})
    endif()
else()
    message(WARNING "SDL2 not found — host preview target skipped (tests still build).")
endif()

# --- Headless frame dumper (visual checks / screenshot baselines) -----------
add_executable(dump_frame tools/dump_frame.cpp)
target_link_libraries(dump_frame PRIVATE malcore)

# Validates a published manifest with the device's own parser (see the file
# banner). Driven by `make manifest` / `make manifest-check` at the repo root.
add_executable(check_manifest tools/check_manifest.cpp)
target_link_libraries(check_manifest PRIVATE malcore)

# --- Native tests -----------------------------------------------------------
enable_testing()
# Globbed: the suite is one gate roster (test_main.cpp) over per-subject units, and
# a new test_<subject>.cpp should cost a file, not a build edit — same rule the
# engine sources follow. CONFIGURE_DEPENDS re-globs when a file appears.
file(GLOB MAL_TEST_SOURCES CONFIGURE_DEPENDS test/test_native/*.cpp)
add_executable(mal_tests ${MAL_TEST_SOURCES})
target_link_libraries(mal_tests PRIVATE malcore)
add_test(NAME mal_tests COMMAND mal_tests)

# A source-level gate rather than a unit test, because what it guards is a
# property of the whole tree: every colour reaches the screen through palColor(),
# which is what keeps a theme swap from becoming a sweep through the screens.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME palette_bindings
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/check_palette_bindings.py)
    # Likewise tree-wide: a comment naming a planning card is describing a
    # different card the moment the board is renumbered, and nothing tells the
    # reader. docs/COMMENT_STANDARD.md is the rule; this is its enforceable half.
    add_test(NAME comment_standard
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/check_comment_standard.py)
    # A gate written but never added to the roster doesn't run, and a green suite
    # says nothing about it. The compiler catches the other direction on its own.
    add_test(NAME test_roster
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/check_test_roster.py)
    # Local, because CI cannot check the file that starts CI: an unparseable
    # workflow completes in zero seconds with no jobs and no logs, so the tier
    # simply stops running and nothing says so. Skips itself without PyYAML.
    add_test(NAME workflows_parse
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/check_workflows.py)
endif()
