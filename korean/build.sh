#!/bin/bash
# Build the Korean translation files into <out dir>:
#   KOREAN.LNG / KOREAN.LNG2 (UTF-8 language files), KOREAN.UI (UI string table), KOREAN.FNT (Hangul font, optional)
# usage: korean/build.sh <languagetool> <out dir> [Galmuri7.ttf Galmuri9.ttf]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
LT="$1"; OUT="$2"
mkdir -p "$OUT"
"$LT" -i "$HERE/KOREAN.TXT" -u -o "$OUT/KOREAN.LNG" -c 1013
"$LT" -i "$HERE/KOREAN2.TXT" -u -o "$OUT/KOREAN.LNG2" -c 1
cp "$HERE/KOREAN.UI" "$OUT/"
if [ -n "$3" ]; then python3 "$HERE/make_font.py" "$3" "$4" "$OUT/KOREAN.FNT"; fi
echo "Korean files in $OUT"
