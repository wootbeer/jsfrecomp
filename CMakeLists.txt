cmake_minimum_required(VERSION 3.20)
project(JediStarfighterRecomp VERSION 0.0.1 LANGUAGES C CXX)

# Jedi Starfighter Recomp: a static recompilation of Star Wars Jedi Starfighter (Xbox).
#
# The executable always builds. When generated code is present in JSF_GEN_DIR
# (tools/recomp.py writes it there from your own default.xbe), it is compiled
# in together with the xboxrecomp kernel layer. Without it you get the
# launcher alone, which is enough to test the game-file setup.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(JSF_RELEASE "A release build: no console window on Windows (tools/package.py)" OFF)
# Diagnosis of code that behaves differently under MSVC: drop its huge-
# function optimisation (see jsf_generated_options).
option(JSF_MSVC_HUGE "MSVC: /d2OptimizeHugeFunctions on the generated code" ON)

# The C runtime linked in, so the exe needs no Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(JSF_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gen" CACHE PATH
    "Directory holding the generated C from tools/recomp.py")

# ── SDL3 (window, dialogs, file I/O, later rendering/audio/input) ────────

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
# Our changes to SDL (patches/SDL/), kept here only: the Vulkan objects
# behind SDL's GPU device, for frame generation (lsfg_present.cpp).
# Applied once; re-running configure finds them already in place.
find_package(Git QUIET)
file(GLOB JSF_SDL_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/SDL/*.patch")
list(SORT JSF_SDL_PATCHES)
foreach(patch ${JSF_SDL_PATCHES})
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --reverse --check "${patch}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL"
        RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
    if(NOT already EQUAL 0)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} apply "${patch}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL"
            RESULT_VARIABLE failed)
        if(failed)
            message(FATAL_ERROR "Could not apply ${patch} to external/SDL. "
                "Reset it with: git -C external/SDL checkout -- .")
        endif()
        message(STATUS "Jedi Starfighter Recomp: applied ${patch}")
    endif()
endforeach()
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# ── Launcher ─────────────────────────────────────────────────────────────

# On Android the game is libmain.so, which SDL's SDLActivity loads and whose
# SDL_main it calls (SDL_main.h renames main); SDL itself is linked in.
if(ANDROID)
    add_library(JediStarfighterRecomp SHARED)
    set_target_properties(JediStarfighterRecomp PROPERTIES OUTPUT_NAME main)
    # The on-screen touch controls (descore's touch module, drawn by
    # android/'s TouchControlsView; no GL here).
    target_sources(JediStarfighterRecomp PRIVATE
        src/android/touch_controls.c
        src/android/descore/touch/descore_touch.c)
    set_source_files_properties(src/android/descore/touch/descore_touch.c
        PROPERTIES COMPILE_DEFINITIONS DESCORE_TOUCH_NO_GL)
    # A custom Vulkan driver (Adreno: Turnip) chosen in the options, loaded
    # with libadrenotools (external/libadrenotools, BSD-2-Clause) in place of
    # the system's: src/android/gpu_driver/ (shared with other projects) and
    # android_gpu.c; SDL takes it through patches/SDL/0003. Its hook
    # libraries, libhook_impl.so and libmain_hook.so, go in the APK too
    # (android/app/build.gradle.kts).
    add_subdirectory(external/libadrenotools ${CMAKE_BINARY_DIR}/adrenotools EXCLUDE_FROM_ALL)
    target_sources(JediStarfighterRecomp PRIVATE
        src/android/android_gpu.c
        src/android/gpu_driver/gpu_driver.c)
    target_link_libraries(JediStarfighterRecomp PRIVATE adrenotools)
else()
    add_executable(JediStarfighterRecomp)
endif()
target_sources(JediStarfighterRecomp PRIVATE
    src/main.c
    src/config.c
    src/input_map.c
    src/assets.c
    src/setup_ui.c
    src/xiso.c
    src/xbe.c
    src/sha256.c
)
target_include_directories(JediStarfighterRecomp PRIVATE src)
target_link_libraries(JediStarfighterRecomp PRIVATE SDL3::SDL3-static)
target_compile_definitions(JediStarfighterRecomp PRIVATE JSF_VERSION="${PROJECT_VERSION}")
# The commit built, for the log's first line (a stale executable shows at
# once). Configure runs again when git's index changes (a commit, checkout
# or pull).
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE JSF_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT JSF_COMMIT)
    set(JSF_COMMIT "unknown")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/index")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/.git/index")
endif()
target_compile_definitions(JediStarfighterRecomp PRIVATE JSF_COMMIT="${JSF_COMMIT}")
if(WIN32)
    enable_language(RC)
    target_sources(JediStarfighterRecomp PRIVATE icons/app.rc)
    # A console during development, so logs and --check output show;
    # releases have none (--log still writes the log).
    if(JSF_RELEASE)
        set_target_properties(JediStarfighterRecomp PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()
if(JSF_RELEASE)
    target_compile_definitions(JediStarfighterRecomp PRIVATE JSF_RELEASE=1)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # The C++ runtime (frame generation) linked in, so the binary needs
        # only the C library, as before (SteamOS and older distributions).
        target_link_options(JediStarfighterRecomp PRIVATE -static-libstdc++ -static-libgcc)
    endif()
    if(MSVC)
        # Function pointers to the bodies, not incremental-link thunks: the
        # crash report names game functions from them.
        target_link_options(JediStarfighterRecomp PRIVATE /INCREMENTAL:NO)
    endif()
endif()

if(MSVC)
    target_compile_options(JediStarfighterRecomp PRIVATE /W4 /wd4100)
    target_compile_definitions(JediStarfighterRecomp PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(JediStarfighterRecomp PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ── Recompiled game code (optional until you have generated it) ──────────

if(EXISTS "${JSF_GEN_DIR}/recomp_dispatch.c")
    message(STATUS "Jedi Starfighter Recomp: generated code found in ${JSF_GEN_DIR}")

    # Only the parts of xboxrecomp we use. Its graphics, audio and input
    # libraries are replaced by our own SDL3 ones (later milestones), so they
    # are not built at all.
    set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/xboxrecomp")

    # Fixes we carry until they are merged upstream (patches/xboxrecomp/).
    # Applied once; re-running configure finds them already in place.
    find_package(Git QUIET)
    file(GLOB JSF_XBOXRECOMP_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/xboxrecomp/*.patch")
    list(SORT JSF_XBOXRECOMP_PATCHES)
    foreach(patch ${JSF_XBOXRECOMP_PATCHES})
        execute_process(
            COMMAND ${GIT_EXECUTABLE} apply --reverse --check "${patch}"
            WORKING_DIRECTORY "${XBOXRECOMP_DIR}"
            RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
        if(NOT already EQUAL 0)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} apply "${patch}"
                WORKING_DIRECTORY "${XBOXRECOMP_DIR}"
                RESULT_VARIABLE failed)
            if(failed)
                message(FATAL_ERROR "Could not apply ${patch} to external/xboxrecomp. "
                    "Reset it with: git -C external/xboxrecomp checkout -- .")
            endif()
            message(STATUS "Jedi Starfighter Recomp: applied ${patch}")
        endif()
    endforeach()
    add_subdirectory(${XBOXRECOMP_DIR}/src/platform ${CMAKE_BINARY_DIR}/xboxrecomp/platform)
    add_subdirectory(${XBOXRECOMP_DIR}/src/kernel ${CMAKE_BINARY_DIR}/xboxrecomp/kernel)

    # The generated code and our hooks are compiled with
    # src/guest/jsf_names.h forced in first: the dispatch becomes
    # jsf_recomp_lookup..., and src/guest/program.c routes the unprefixed
    # names the kernel layer calls to it (kept from Orta Recomp, which linked
    # two programs into one executable).
    function(jsf_force_include target header)
        if(MSVC)
            target_compile_options(${target} PRIVATE "/FI${header}")
        else()
            # SHELL: keeps "-include file" together: CMake drops a repeated
            # -include otherwise, and the second header's file name is then
            # taken for a source.
            target_compile_options(${target} PRIVATE "SHELL:-include \"${header}\"")
        endif()
    endfunction()
    file(GLOB JSF_GEN_SOURCES CONFIGURE_DEPENDS "${JSF_GEN_DIR}/*.c")
    # An object library rather than a static one: the kernel layer calls back
    # into the generated dispatch table and our hooks, a circular dependency
    # that single-pass linkers cannot resolve between static archives.
    # The XDK in place of the Xbox's (Direct3D, DirectSound, input):
    # src/hle/.
    file(GLOB JSF_HLE_SOURCES CONFIGURE_DEPENDS src/hle/*.c)
    # The generated code, src/hle/ and the game's own hooks (src/guest/).
    add_library(jsf_recomp OBJECT ${JSF_GEN_SOURCES} ${JSF_HLE_SOURCES}
        src/guest/guest_manual.c
        src/guest/video_hooks.c
        src/guest/crt_check.c
        src/guest/widescreen.c)
    jsf_force_include(jsf_recomp "${CMAKE_CURRENT_SOURCE_DIR}/src/guest/jsf_names.h")
    # The host's own code, the same whichever program runs.
    add_library(jsf_host OBJECT
        src/guest/crash_report.c
        src/guest/vsh.c
        src/audio/mixer.c
        src/audio/reverb.c
        src/gfx/renderer.c
        src/gfx/overlay.c
        src/gfx/vsh_spec.c
        src/gfx/xtex.c)
    target_include_directories(jsf_host PUBLIC "${JSF_GEN_DIR}" src)
    target_link_libraries(jsf_host PUBLIC xbox_kernel SDL3::SDL3-static)
    target_link_libraries(jsf_recomp PUBLIC jsf_host)
    if(WIN32)
        target_link_libraries(jsf_host PUBLIC dbghelp)
    endif()
    # glslang (external/glslang, pinned): the game's vertex programs made into
    # shaders of their own at run time (src/gfx/vsh_spec.c). The compiler
    # only: no optimizer, HLSL, tools or tests. Without it they are
    # interpreted on the GPU, as before.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glslang/CMakeLists.txt")
        set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
        set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
        set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
        set(ENABLE_GLSLANG_JS OFF CACHE BOOL "" FORCE)
        set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
        set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
        set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory(external/glslang ${CMAKE_BINARY_DIR}/glslang EXCLUDE_FROM_ALL)
        target_link_libraries(jsf_host PUBLIC glslang::glslang glslang::glslang-default-resource-limits)
        target_compile_definitions(jsf_host PRIVATE JSF_HAVE_GLSLANG=1)
        message(STATUS "Jedi Starfighter Recomp: vertex programs made into their own shaders (glslang)")
    else()
        message(STATUS "Jedi Starfighter Recomp: no external/glslang (git submodule update --init); "
                       "vertex programs are interpreted on the GPU")
    endif()
    # The vertex program interpreter rounds every operation, as the NV2A
    # does: no fused multiply-add (fp:precise is MSVC's default already).
    if(NOT MSVC)
        set_source_files_properties(src/guest/vsh.c PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
    endif()
    # The options for the generated code.
    function(jsf_generated_options target)
    if(MSVC)
        # /d2OptimizeHugeFunctions: MSVC leaves a function above its size
        # limit unoptimised (warning C4883, hidden by /w), and the generated
        # code has such functions -- the movie decoder among them ran so
        # slowly that movies fell to 20 fps and stopped (GCC and Clang have
        # no such limit). The build takes longer for it.
        target_compile_options(${target} PRIVATE /bigobj /w)
        if(JSF_MSVC_HUGE)
            target_compile_options(${target} PRIVATE /d2OptimizeHugeFunctions)
        endif()
    else()
        # The generated code reads guest memory through casted pointers and
        # relies on 32-bit wraparound, so neither assumption may be optimised on.
        target_compile_options(${target} PRIVATE
            -fno-strict-aliasing -fwrapv -w)
    endif()
    endfunction()
    jsf_generated_options(jsf_recomp)
    jsf_generated_options(jsf_host)

    # Lossless Scaling frame generation (lib/lsfg, from BattleTanx: Global
    # Assault Recompiled; the player's own Lossless.dll at run time). Its
    # shader translator needs SPIR-V's headers: the external/SPIRV-Headers
    # submodule (pinned, new enough), else JSF_SPIRV_HEADERS, the Vulkan
    # SDK's or the system's. Without any, the game builds without it.
    set(JSF_SPIRV_HEADERS "" CACHE PATH "Folder holding spirv/unified1/spirv.hpp (SPIRV-Headers' include)")
    set(JSF_SPIRV_FOUND "")
    # Not on Android: on the RP6 (Adreno 740) it took the GPU 40-68 ms a
    # frame at 1080p, more with a smaller flow scale (android/PLAN.md).
    foreach(dir "${JSF_SPIRV_HEADERS}" "${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Headers/include"
                "$ENV{VULKAN_SDK}/include" "$ENV{VULKAN_SDK}/Include" "/usr/include")
        if(NOT ANDROID AND NOT JSF_SPIRV_FOUND AND dir AND EXISTS "${dir}/spirv/unified1/spirv.hpp")
            set(JSF_SPIRV_FOUND "${dir}")
        endif()
    endforeach()
    if(JSF_SPIRV_FOUND)
        message(STATUS "Jedi Starfighter Recomp: Lossless Scaling frame generation built (SPIR-V headers in ${JSF_SPIRV_FOUND})")
        # Vulkan's headers as SDL carries them, and a stand-in for volk.
        add_library(jsf_vk STATIC src/gfx/vk/vk_load.c)
        target_include_directories(jsf_vk PUBLIC src/gfx/vk "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL/src/video/khronos")
        target_compile_definitions(jsf_vk PUBLIC VK_NO_PROTOTYPES)
        target_link_libraries(jsf_vk PUBLIC SDL3::SDL3-static)
        set(LSFG_VULKAN_TARGET jsf_vk)
        set(LSFG_SPIRV_INCLUDE_DIR "${JSF_SPIRV_FOUND}")
        add_subdirectory(lib/lsfg ${CMAKE_BINARY_DIR}/lsfg)
        target_sources(jsf_host PRIVATE src/gfx/lsfg_present.cpp)
        target_compile_definitions(jsf_host PUBLIC JSF_HAVE_LSFG=1)
        target_link_libraries(jsf_host PUBLIC lsfg)
    elseif(ANDROID)
        message(STATUS "Jedi Starfighter Recomp: no Lossless Scaling frame generation on Android")
    else()
        message(STATUS "Jedi Starfighter Recomp: no SPIR-V headers (run git submodule update --init, or set JSF_SPIRV_HEADERS); "
                       "building without Lossless Scaling frame generation")
    endif()

    target_sources(JediStarfighterRecomp PRIVATE src/guest/guest.c src/guest/program.c src/guest/relaunch.c)
    target_compile_definitions(JediStarfighterRecomp PRIVATE JSF_HAVE_RECOMP=1)
    # Object libraries give their objects only to what links them directly.
    target_link_libraries(JediStarfighterRecomp PRIVATE jsf_recomp jsf_host)
else()
    message(STATUS "Jedi Starfighter Recomp: no generated code in ${JSF_GEN_DIR}; "
                   "building the launcher only (run tools/recomp.py first)")
endif()
