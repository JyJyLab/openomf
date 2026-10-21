# OpenOMF Android build: libmain.so loaded by SDLActivity (arm64-v8a)
# Dependencies: android/build_deps.sh -> android/work/prefix
cmake_minimum_required(VERSION 3.16)
project(openomf_android C)

get_filename_component(OMF "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(OMF_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/work/prefix" CACHE PATH "dependency prefix (android/build_deps.sh)")

file(GLOB_RECURSE SRC LIST_DIRECTORIES OFF "${OMF}/src/*/*.c")
list(FILTER SRC EXCLUDE REGEX "/src/audio/backends/.*/")
list(FILTER SRC EXCLUDE REGEX "/src/video/renderers/.*/")
list(FILTER SRC EXCLUDE REGEX "(sound|music)_sources/null_")
file(GLOB_RECURSE SDL_AUDIO "${OMF}/src/audio/backends/sdl/*.c")
file(GLOB_RECURSE GL3 "${OMF}/src/video/renderers/opengl3/*.c")
list(APPEND SRC ${SDL_AUDIO} ${GL3} "${OMF}/src/main.c" "${OMF}/src/engine.c")

add_library(main SHARED ${SRC})
target_compile_definitions(main PRIVATE PNG_FOUND)
set_source_files_properties("${OMF}/src/audio/audio.c" PROPERTIES COMPILE_DEFINITIONS "ENABLE_SDL_AUDIO_BACKEND")
set_source_files_properties("${OMF}/src/video/video.c" PROPERTIES COMPILE_DEFINITIONS "ENABLE_OPENGL3_RENDERER")
set_source_files_properties("${OMF}/src/game/utils/version.c" PROPERTIES COMPILE_DEFINITIONS "V_MAJOR=0;V_MINOR=8;V_PATCH=7;V_LABEL=\"-android\"")
set_source_files_properties("${OMF}/src/main.c" PROPERTIES COMPILE_DEFINITIONS "SHA1_HASH=\"de19acf0\"")
target_compile_options(main PRIVATE -std=gnu11 -O2 -Wno-unused-parameter "-ffile-prefix-map=${OMF}/=")
target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
    ${OMF}/src
    ${OMF}/src/vendored/argtable
    ${OMF}/src/vendored
    ${OMF_DEPS_PREFIX}/include
    ${OMF_DEPS_PREFIX}/include/SDL2
)
target_link_libraries(main PRIVATE
    ${OMF_DEPS_PREFIX}/lib/libSDL2_mixer.a
    ${OMF_DEPS_PREFIX}/lib/libxmp.a
    ${OMF_DEPS_PREFIX}/lib/libenet.a
    ${OMF_DEPS_PREFIX}/lib/libconfuse.a
    ${OMF_DEPS_PREFIX}/lib/libpng16.a
    z
    ${OMF_DEPS_PREFIX}/lib/libSDL2.so
    GLESv3 EGL log android m
)
