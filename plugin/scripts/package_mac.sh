#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${1:-$ROOT/build-macos}"
ARCH="${2:-$(uname -m)}"
VERSION="0.1.0"
PAYLOAD="$BUILD_DIR/package-root"
ENGINE="$BUILD_DIR/engine-dist/DettatoEngine"
VST3="$BUILD_DIR/Dettato_artefacts/Release/VST3/Dettato.vst3"
AU="$BUILD_DIR/Dettato_artefacts/Release/AU/Dettato.component"
AAX="$BUILD_DIR/Dettato_artefacts/Release/AAX/Dettato.aaxplugin"

test -d "$ENGINE"
test -d "$VST3"
test -d "$AU"
rm -rf "$PAYLOAD"
mkdir -p \
  "$PAYLOAD/Library/Audio/Plug-Ins/VST3" \
  "$PAYLOAD/Library/Audio/Plug-Ins/Components" \
  "$PAYLOAD/Library/Application Support/KobiMusic/Dettato" \
  "$PAYLOAD/Library/Documentation/KobiMusic/Dettato"
cp -R "$VST3" "$PAYLOAD/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU" "$PAYLOAD/Library/Audio/Plug-Ins/Components/"
cp -R "$ENGINE" "$PAYLOAD/Library/Application Support/KobiMusic/Dettato/"
cp "$ROOT/LICENSE" "$ROOT/NOTICE" "$ROOT/plugin/LICENSE" "$ROOT/plugin/THIRD_PARTY_NOTICES.md" \
  "$PAYLOAD/Library/Documentation/KobiMusic/Dettato/"

if [[ "${DETTATO_PACKAGE_AAX:-0}" == "1" ]]; then
  test -d "$AAX"
  echo "Packaging AAX. It must already be signed with Avid's official signing tool."
  mkdir -p "$PAYLOAD/Library/Application Support/Avid/Audio/Plug-Ins"
  cp -R "$AAX" "$PAYLOAD/Library/Application Support/Avid/Audio/Plug-Ins/"
fi

if [[ -n "${MACOS_DEVELOPER_ID_APPLICATION:-}" ]]; then
  while IFS= read -r file; do
    if file -b "$file" | grep -q 'Mach-O'; then
      codesign --force --options runtime --timestamp \
        --sign "$MACOS_DEVELOPER_ID_APPLICATION" "$file"
    fi
  done < <(find "$PAYLOAD/Library/Application Support/KobiMusic/Dettato/DettatoEngine" -type f)
  codesign --force --options runtime --timestamp \
    --sign "$MACOS_DEVELOPER_ID_APPLICATION" "$PAYLOAD/Library/Audio/Plug-Ins/VST3/Dettato.vst3"
  codesign --force --options runtime --timestamp \
    --sign "$MACOS_DEVELOPER_ID_APPLICATION" "$PAYLOAD/Library/Audio/Plug-Ins/Components/Dettato.component"
  codesign --verify --deep --strict --verbose=2 "$PAYLOAD/Library/Audio/Plug-Ins/VST3/Dettato.vst3"
  codesign --verify --deep --strict --verbose=2 "$PAYLOAD/Library/Audio/Plug-Ins/Components/Dettato.component"
fi

OUTPUT="$BUILD_DIR/Dettato-$VERSION-macOS-$ARCH-unsigned.pkg"
ARGS=(--root "$PAYLOAD" --identifier com.kobimusic.dettato --version "$VERSION" --install-location /)
if [[ -n "${MACOS_DEVELOPER_ID_INSTALLER:-}" ]]; then
  ARGS+=(--sign "$MACOS_DEVELOPER_ID_INSTALLER")
  OUTPUT="$BUILD_DIR/Dettato-$VERSION-macOS-$ARCH.pkg"
fi
pkgbuild "${ARGS[@]}" "$OUTPUT"

if [[ -n "${MACOS_DEVELOPER_ID_INSTALLER:-}" && -n "${APPLE_NOTARY_KEY_ID:-}" ]]; then
  xcrun notarytool submit "$OUTPUT" \
    --key "$APPLE_NOTARY_PRIVATE_KEY_PATH" \
    --key-id "$APPLE_NOTARY_KEY_ID" \
    --issuer "$APPLE_NOTARY_ISSUER_ID" \
    --wait
  xcrun stapler staple "$OUTPUT"
  spctl -a -vv -t install "$OUTPUT"
else
  echo "Unsigned package created. It is a development artifact, not a notarized release."
fi

echo "$OUTPUT"
