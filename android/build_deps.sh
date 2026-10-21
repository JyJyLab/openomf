#!/bin/bash
# OpenOMF Android dependencies (arm64-v8a, API 24) -> android/work/prefix
#   SDL2 2.30.9 (shared), SDL2_mixer 2.8.0, libxmp 4.6.0, enet 1.3.18, libconfuse 3.3, libpng 1.6.44 (static)
# Needs: ANDROID_NDK_HOME, cmake, ninja, curl, make
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
NDK="${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME (tested with NDK r27c)}"
ABI=arm64-v8a
API=24
WORK="$HERE/work"
SRC=$WORK/src
P=$WORK/prefix
B=$WORK/build
mkdir -p $SRC $P $B
fetch() { [ -f "$SRC/$2" ] || curl -sSL -o "$SRC/$2" "$1"; tar xzf "$SRC/$2" -C "$SRC"; }
fetch https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz SDL2-2.30.9.tar.gz
fetch https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-2.8.0.tar.gz SDL2_mixer-2.8.0.tar.gz
fetch https://github.com/libxmp/libxmp/releases/download/libxmp-4.6.0/libxmp-4.6.0.tar.gz libxmp-4.6.0.tar.gz
fetch https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz enet-1.3.18.tar.gz
fetch https://github.com/libconfuse/libconfuse/releases/download/v3.3/confuse-3.3.tar.gz confuse-3.3.tar.gz
fetch https://download.sourceforge.net/libpng/libpng-1.6.44.tar.gz libpng-1.6.44.tar.gz

TC="-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$P -DCMAKE_PREFIX_PATH=$P -DCMAKE_FIND_ROOT_PATH=$P -G Ninja"

# SDL2: shared (SDLActivity loads libSDL2.so first)
cmake -S $SRC/SDL2-2.30.9 -B $B/sdl2 $TC -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
ninja -C $B/sdl2 install

# SDL2_mixer: static, no external codecs (music = libxmp, sounds = original data)
cmake -S $SRC/SDL2_mixer-2.8.0 -B $B/mixer $TC -DBUILD_SHARED_LIBS=OFF -DSDL2MIXER_VENDORED=OFF -DSDL2MIXER_SAMPLES=OFF \
  -DSDL2MIXER_CMD=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_GME=OFF -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MP3=OFF \
  -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_VORBIS=OFF -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_WAVE=ON
ninja -C $B/mixer install

cmake -S $SRC/libxmp-4.6.0 -B $B/xmp $TC -DBUILD_SHARED=OFF -DBUILD_STATIC=ON
ninja -C $B/xmp install

# enet has no install target
cmake -S $SRC/enet-1.3.18 -B $B/enet $TC
ninja -C $B/enet
cp $B/enet/libenet.a $P/lib/
cp -r $SRC/enet-1.3.18/include/enet $P/include/

# libconfuse: autotools cross build, -fPIC because it is linked into libmain.so
TCBIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
mkdir -p $B/confuse && cd $B/confuse
CFLAGS="-O2 -fPIC" CC=$TCBIN/aarch64-linux-android$API-clang AR=$TCBIN/llvm-ar RANLIB=$TCBIN/llvm-ranlib \
  $SRC/confuse-3.3/configure --host=aarch64-linux-android --prefix=$P --disable-shared --enable-static --disable-nls --disable-examples >/dev/null
make -j2 -C src >/dev/null && make -C src install >/dev/null
cd "$HERE"

# libpng (zlib from the NDK)
cmake -S $SRC/libpng-1.6.44 -B $B/png $TC -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_ARM_NEON=on
ninja -C $B/png install
echo "dependencies installed in $P"
