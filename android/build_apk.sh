#!/bin/bash
# Build the OpenOMF Android APK without Gradle (aapt2 + javac + d8 + zipalign + apksigner).
#
#   ANDROID_HOME      Android SDK with platforms;android-34 and build-tools;34.0.0 (+ cmdline-tools for d8)
#   ANDROID_NDK_HOME  Android NDK (tested r27c)
#   OMF_DATA          folder with the original One Must Fall 2097 game files (not included in this repository)
#   OMF_DESKTOP_BUILD desktop OpenOMF build dir that contains resources/ (openomf.bk, *.LNG2 ...) and languagetool,
#                     default: ../build-linux
#   KEYSTORE / KEYSTORE_PASS  signing key (a new personal key is created in android/work if missing)
#
# usage: android/build_apk.sh [version]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OMF="$(cd "$HERE/.." && pwd)"
SDK="${ANDROID_HOME:?set ANDROID_HOME}"
NDK="${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME}"
DATA="${OMF_DATA:?set OMF_DATA to the original game data folder}"
DESK="${OMF_DESKTOP_BUILD:-$OMF/build-linux}"
BT=$SDK/build-tools/34.0.0
JAR=$SDK/platforms/android-34/android.jar
D8=$SDK/cmdline-tools/latest/bin/d8   # build-tools 34 d8 fails on JDK 21 class files
WORK=$HERE/work
PREFIX=$WORK/prefix
OUT=$WORK/out
VER=${1:-0.6}
STRIP=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip

# 1) native library
cmake -S $HERE -B $WORK/build-main -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release -DOMF_DEPS_PREFIX=$PREFIX >/dev/null
ninja -C $WORK/build-main

rm -rf $OUT && mkdir -p $OUT/assets/omf/resources $OUT/assets/omf/shaders $OUT/classes $OUT/lib/arm64-v8a $OUT/gen

# 2) game data (DOS executables, setup and modem tools are left out)
for f in "$DATA"/*; do
  case "$(basename "$f")" in
    *.EXE|*.exe|*.PIF|*.BAT|MODEMS.TXT|NETTERM.INT|NETEPIC.BBS|*.CHR) ;;
    *) cp "$f" $OUT/assets/omf/resources/ ;;
  esac
done
cp "$DESK"/resources/* $OUT/assets/omf/resources/
# Korean translation (KOREAN.LNG/.LNG2/.UI/.FNT); the game switches to Korean once on first run
"$OMF/korean/build.sh" "$DESK/languagetool" $OUT/assets/omf/resources
cp -r $OMF/shaders/* $OUT/assets/omf/shaders/
( cd $OUT/assets/omf && { echo "data-$VER-$(cat resources/* shaders/*/* shaders/* 2>/dev/null | md5sum | cut -c1-12)"; find resources shaders -type f | sort; } > files.txt )

# 3) native libraries
$STRIP -o $OUT/lib/arm64-v8a/libmain.so $WORK/build-main/libmain.so
$STRIP -o $OUT/lib/arm64-v8a/libSDL2.so $PREFIX/lib/libSDL2.so

# 4) resources + manifest
$BT/aapt2 compile --dir $HERE/app/res -o $OUT/res.zip
$BT/aapt2 link -o $OUT/base.apk -I $JAR --manifest $HERE/app/AndroidManifest.xml -A $OUT/assets \
  --min-sdk-version 24 --target-sdk-version 34 --version-name "$VER" --java $OUT/gen $OUT/res.zip

# 5) Java -> dex
javac --release 8 -nowarn -cp $JAR -d $OUT/classes $(find $HERE/app/java $OUT/gen -name '*.java') 2>&1 | grep -v '^Note:' || true
$D8 --min-api 24 --lib $JAR --output $OUT $(find $OUT/classes -name '*.class')

# 6) package, align, sign
cd $OUT && cp base.apk unsigned.apk && zip -q unsigned.apk classes.dex && zip -q -r unsigned.apk lib
$BT/zipalign -f -p 4 unsigned.apk aligned.apk
KS="${KEYSTORE:-$WORK/personal.keystore}"
PASS="${KEYSTORE_PASS:-android}"
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass "$PASS" -keypass "$PASS" -alias omf -keyalg RSA -keysize 2048 \
  -validity 10000 -dname "CN=OpenOMF personal build" >/dev/null 2>&1
$BT/apksigner sign --ks "$KS" --ks-pass pass:"$PASS" --key-pass pass:"$PASS" --out OpenOMF-$VER.apk aligned.apk
$BT/apksigner verify OpenOMF-$VER.apk
echo "$OUT/OpenOMF-$VER.apk"
