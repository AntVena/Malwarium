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

# --- Web demo (Emscripten) --------------------------------------------------
# emcmake configures this branch. The engine is the same malcore the firmware
# links; MALWARIUM_DEMO is what shuts the slots a browser cannot honour
# (include/demo_config.h), and it goes on malcore because Game::slotLocked lives
# there. src/generated/demo_save.cpp is the baked seed, produced by a NATIVE build
# of gen_demo_save before this tree is configured — see tools/build_web.sh.
if(EMSCRIPTEN)
    target_compile_definitions(malcore PUBLIC MALWARIUM_DEMO)

    add_executable(malwarium_web
        src/platform/web/main_web.cpp
        ${CMAKE_SOURCE_DIR}/src/generated/demo_save.cpp)
    target_link_libraries(malwarium_web PRIVATE malcore)
    set_target_properties(malwarium_web PROPERTIES OUTPUT_NAME "malwarium")

    target_compile_options(malwarium_web PRIVATE -O3)
    target_link_options(malwarium_web PRIVATE
        -O3
        # The atlas alone is ~1.2MB of RGB565, and the engine allocates its
        # framebuffers on top; growth keeps that from being a fixed reservation
        # every visitor downloads the cost of.
        -sALLOW_MEMORY_GROWTH=1
        # The shell page's controls (see main_web.cpp's entry points) plus the
        # runtime helper they are called through.
        -sEXPORTED_FUNCTIONS=['_main','_mal_button','_mal_cycle_ui_mode','_mal_reset_demo','_mal_flush_save']
        # UTF8ToString/stringToNewUTF8 are used by the EM_JS bodies in
        # save_store_web.h; naming them here keeps -O3 from eliminating them.
        -sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString','stringToNewUTF8']
        # The page supplies its own canvas and chrome (pages/play/index.html), so
        # emcc emits only the loader + wasm. MODULARIZE hands the page a factory it
        # awaits, rather than a global that races the DOM being ready.
        -sMODULARIZE=1
        -sEXPORT_NAME=Malwarium)
    return()
endif()

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

# --- Web demo seed ----------------------------------------------------------
# Bakes the wasm demo's starting save by running the engine's own serializer (see
# the file banner). A HOST tool even when the demo it feeds is a wasm build, which
# is why tools/build_web.sh configures a native tree for this before running emcmake.
add_executable(gen_demo_save tools/gen_demo_save.cpp)
target_link_libraries(gen_demo_save PRIVATE malcore)

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
    # The Worm line's sheets are generated, and the generator owns them: a hand edit
    # to a committed PNG is a change nothing can reproduce, so the next run of the
    # tool silently reverts it. This fails instead, and names the command that fixes
    # it. Costs nothing when no worm-line art has moved.
    add_test(NAME worm_art_recipes
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/gen_worm_art.py --check)
    # Local, because CI cannot check the file that starts CI: an unparseable
    # workflow completes in zero seconds with no jobs and no logs, so the tier
    # simply stops running and nothing says so. Skips itself without PyYAML.
    add_test(NAME workflows_parse
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/check_workflows.py)
endif()
