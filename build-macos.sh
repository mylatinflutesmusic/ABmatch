#!/usr/bin/env bash
# Builds ABmatch as a macOS 10.13 (High Sierra) compatible VST3.
#
# High Sierra is Intel-only, so the plug-in is built x86_64 with a 10.13
# deployment target (no arm64 slice, no APIs newer than 10.13).
#
# Requirements on the build Mac: Xcode command line tools, CMake 3.22+.
# Build with Xcode 14 or 15 if possible: very new Xcode versions no longer
# produce binaries that launch on older macOS releases even when the
# deployment target is set correctly.
#
# Usage:  ./build-macos.sh
# Env:    BUILD_DIR (default build-macos), SKIP_INSTALL=1 to leave the bundle
#         in the build tree.

set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR=${BUILD_DIR:-build-macos}
DEPLOYMENT_TARGET=10.13
ARCHS=x86_64

echo "=== ABmatch: macOS ${DEPLOYMENT_TARGET} / ${ARCHS} ==="

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

cmake --build "$BUILD_DIR" --config Release --target ABmatch_VST3 --parallel

BUNDLE="$BUILD_DIR/ABmatch_artefacts/Release/VST3/ABmatch.vst3"

if [ ! -d "$BUNDLE" ]; then
    echo "error: expected bundle not found at $BUNDLE" >&2
    exit 1
fi

BINARY="$BUNDLE/Contents/MacOS/ABmatch"

echo
echo "=== Build metadata ==="
lipo -info "$BINARY"
vtool -show-build "$BINARY" 2>/dev/null || otool -l "$BINARY" | grep -A4 -E "LC_BUILD_VERSION|LC_VERSION_MIN_MACOSX"

echo
echo "=== Bundle minimum OS ==="
plutil -p "$BUNDLE/Contents/Info.plist" | grep -i -E "LSMinimumSystemVersion|CFBundleShortVersionString" || true

MINOS=$(vtool -show-build "$BINARY" 2>/dev/null | awk '/minos/ {print $2; exit}')

if [ -n "${MINOS:-}" ]; then
    if ! awk -v a="$MINOS" -v b="$DEPLOYMENT_TARGET" 'BEGIN { exit !(a > b) }'; then
        echo "minimum OS $MINOS is compatible with ${DEPLOYMENT_TARGET}"
    else
        echo "warning: binary targets $MINOS, which will not load on ${DEPLOYMENT_TARGET}" >&2
    fi
fi

if [ "${SKIP_INSTALL:-0}" = "1" ]; then
    echo
    echo "Skipping install; bundle is at $BUNDLE"
    exit 0
fi

DEST="${HOME}/Library/Audio/Plug-Ins/VST3"
mkdir -p "$DEST"
rm -rf "$DEST/ABmatch.vst3"
cp -R "$BUNDLE" "$DEST/"

# Ad-hoc signature: unquarantined local builds load without one, but signing
# keeps AudioComponentRegistrar from complaining on some systems.
codesign --force --deep --sign - "$DEST/ABmatch.vst3" >/dev/null 2>&1 \
    || echo "note: ad-hoc codesign skipped"

echo
echo "Installed: $DEST/ABmatch.vst3"
echo "Rescan plug-ins in your host (REAPER: Options > Preferences > Plug-ins > Re-scan)."
