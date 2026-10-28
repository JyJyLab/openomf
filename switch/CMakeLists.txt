# OpenOMF Nintendo Switch build: openomf.nro (devkitPro, libnx). OpenGL ES 3.0 through the Android
# shim (android/shim/epoxy/gl.h), like the Android build.
# Dependencies: libxmp and libconfuse built for the Switch into OMF_DEPS_PREFIX; SDL2, SDL2_mixer,
# libpng, enet and mesa (EGL, GLESv2) from devkitPro portlibs.
# OMF_ROMFS: a folder packed into the nro, holding omf/resources (game data, OpenOMF resources),
# omf/shaders and omf/mods.
cmake_minimum_required(VERSION 3.16)
project(openomf_switch C)

get_filename_component(OMF "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(OMF_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/work/prefix" CACHE PATH "libxmp and libconfuse for the Switch")
set(OMF_ROMFS "" CACHE PATH "folder packed into the nro as romfs")
set(OMF_NX_VERSION "1.0.2" CACHE STRING "version shown in the homebrew menu")

file(GLOB_RECURSE SRC LIST_DIRECTORIES OFF "${OMF}/src/*/*.c")
list(FILTER SRC EXCLUDE REGEX "/src/audio/backends/.*/")
list(FILTER SRC EXCLUDE REGEX "/src/video/renderers/.*/")
list(FILTER SRC EXCLUDE REGEX "(sound|music)_sources/null_")
file(GLOB_RECURSE SDL_AUDIO "${OMF}/src/audio/backends/sdl/*.c")
file(GLOB_RECURSE GL3 "${OMF}/src/video/renderers/opengl3/*.c")
list(APPEND SRC ${SDL_AUDIO} ${GL3} "${OMF}/src/main.c" "${OMF}/src/engine.c")
list(APPEND SRC "${CMAKE_CURRENT_SOURCE_DIR}/stubs.c")
list(APPEND SRC "${CMAKE_CURRENT_SOURCE_DIR}/nx_debug.c")   # debug log file and crash report

add_executable(openomf ${SRC})
target_compile_definitions(openomf PRIVATE PNG_FOUND)
set_source_files_properties("${OMF}/src/audio/audio.c" PROPERTIES COMPILE_DEFINITIONS "ENABLE_SDL_AUDIO_BACKEND")
set_source_files_properties("${OMF}/src/video/video.c" PROPERTIES COMPILE_DEFINITIONS "ENABLE_OPENGL3_RENDERER")
set_source_files_properties("${OMF}/src/game/utils/version.c" PROPERTIES COMPILE_DEFINITIONS "V_MAJOR=0;V_MINOR=8;V_PATCH=7;V_LABEL=\"-switch\"")
set_source_files_properties("${OMF}/src/main.c" PROPERTIES COMPILE_DEFINITIONS "SHA1_HASH=\"de19acf0\"")
target_compile_options(openomf PRIVATE -std=gnu11 -O2 -Wno-unused-parameter -fno-omit-frame-pointer "-ffile-prefix-map=${OMF}/=")
target_include_directories(openomf PRIVATE
    ${OMF}/android/shim
    ${OMF}/src
    ${OMF}/src/vendored/argtable
    ${OMF}/src/vendored
    ${OMF_DEPS_PREFIX}/include
    ${DEVKITPRO}/portlibs/switch/include
    ${DEVKITPRO}/portlibs/switch/include/SDL2
)
target_link_directories(openomf PRIVATE ${OMF_DEPS_PREFIX}/lib ${DEVKITPRO}/portlibs/switch/lib)
target_link_libraries(openomf PRIVATE
    SDL2_mixer vorbisidec modplug mpg123 opusfile opus ogg
    xmp confuse enet png16 z
    SDL2 EGL GLESv2 glapi drm_nouveau nx stdc++ m
)

nx_generate_nacp(openomf.nacp NAME "OpenOMF 2097" AUTHOR "OpenOMF" VERSION "${OMF_NX_VERSION}")
if(OMF_ROMFS)
    nx_create_nro(openomf NACP openomf.nacp ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg" ROMFS "${OMF_ROMFS}")
else()
    nx_create_nro(openomf NACP openomf.nacp ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg")
endif()
