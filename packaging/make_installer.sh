#!/bin/bash
# Build a macOS .pkg installer for Serge Animal TKB (v2, or the legacy v1).
#
#   packaging/make_installer.sh v2 [--build]   # v2 from build/ (--build: configure + build first)
#   packaging/make_installer.sh v1 [V1_DIR]    # v1 from V1_DIR (default ~/Library/Audio/Plug-Ins)
#
# Output: dist/SergeAnimalTKB-<product>-<version>-macOS.pkg (+ a .zip of the raw bundles).
# The installer offers AU + VST3 (+ the v2 standalone app) as separate choices and
# installs to /Library/Audio/Plug-Ins/{Components,VST3} and /Applications.
#
# Signing: bundles are ad-hoc signed (no Apple Developer ID). Files installed by
# a .pkg are not quarantined, so the plugins load normally once the installer
# has been opened (right-click > Open the first time). If DEVELOPER_ID_APP /
# DEVELOPER_ID_INSTALLER / NOTARY_PROFILE are set, the script signs + notarizes
# instead (see packaging/INSTALL.md, "Notarized builds").
set -euo pipefail

PRODUCT="${1:-v2}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
DIST="$ROOT/dist"
WORK="$(mktemp -d /tmp/satk-pkgwork.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

case "$PRODUCT" in
  v2)
    VERSION="$(sed -n 's/^project(SergeAnimalTKB VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt")"
    NAME="Serge Animal TKB v2"
    ID="com.renkydenk.SergeAnimalTKB2"
    if [[ "${2:-}" == "--build" ]]; then
      cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
      cmake --build "$ROOT/build" -j "$(sysctl -n hw.ncpu)" --target SergeAnimalTKB_All
    fi
    ART="$ROOT/build/SergeAnimalTKB_artefacts/Release"
    AU="$ART/AU/$NAME.component"; VST3="$ART/VST3/$NAME.vst3"; APP="$ART/Standalone/$NAME.app"
    ;;
  v1)
    VERSION="1.0.0"
    NAME="SergeAnimalTKB"
    ID="com.renkydenk.SergeAnimalTKB"
    V1_DIR="${2:-$HOME/Library/Audio/Plug-Ins}"
    AU="$V1_DIR/Components/$NAME.component"; VST3="$V1_DIR/VST3/$NAME.vst3"; APP=""
    ;;
  *) echo "usage: $0 v2 [--build] | v1 [V1_DIR]" >&2; exit 2 ;;
esac

for b in "$AU" "$VST3" ${APP:+"$APP"}; do
  [[ -d "$b" ]] || { echo "missing bundle: $b" >&2; exit 1; }
  exe="$(find "$b/Contents/MacOS" -type f | head -1)"
  archs="$(lipo -archs "$exe")"
  echo "  $(basename "$b"): $archs"
  [[ "$archs" == *arm64* && "$archs" == *x86_64* ]] || echo "  WARNING: $(basename "$b") is not universal ($archs)" >&2
done

SIGN_APP="${DEVELOPER_ID_APP:--}"          # "-" = ad-hoc
SIGN_PKG="${DEVELOPER_ID_INSTALLER:-}"

# stage, sign, and make one component package per bundle (non-relocatable, so
# the installer always writes to the standard folder instead of "updating" a
# copy it found elsewhere on disk)
mkdir -p "$WORK/pkgs" "$WORK/res" "$DIST"
component () { # <bundle> <install-location> <pkg-id>
  local bundle="$1" loc="$2" pid="$3" stage="$WORK/stage-$3"
  mkdir -p "$stage"
  ditto "$bundle" "$stage/$(basename "$bundle")"
  if [[ "$SIGN_APP" == "-" ]]; then   # ad-hoc: no hardened runtime (hosts must be able to load it)
    codesign --force --deep -s - "$stage/$(basename "$bundle")"
  else                                # Developer ID: hardened runtime + timestamp, required for notarization
    codesign --force --deep --options runtime --timestamp -s "$SIGN_APP" "$stage/$(basename "$bundle")"
  fi
  pkgbuild --analyze --root "$stage" "$WORK/$pid.plist" >/dev/null
  plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$pid.plist"
  pkgbuild --root "$stage" --component-plist "$WORK/$pid.plist" \
           --install-location "$loc" --identifier "$pid" --version "$VERSION" \
           "$WORK/pkgs/$pid.pkg" >/dev/null
}
component "$AU"   "/Library/Audio/Plug-Ins/Components" "$ID.au"
component "$VST3" "/Library/Audio/Plug-Ins/VST3"       "$ID.vst3"
[[ -n "$APP" ]] && component "$APP" "/Applications"     "$ID.app"

# installer UI text + distribution (choices for each component)
sed -e "s/@NAME@/$NAME/g" -e "s/@VERSION@/$VERSION/g" "$HERE/resources/welcome.html" > "$WORK/res/welcome.html"
cp "$HERE/resources/conclusion.html" "$WORK/res/conclusion.html"
{
  cat <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$NAME $VERSION</title>
  <organization>com.renkydenk</organization>
  <welcome file="welcome.html" mime-type="text/html"/>
  <conclusion file="conclusion.html" mime-type="text/html"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <os-version min="11.0"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
EOF
  [[ -n "$APP" ]] && echo '    <line choice="app"/>'
  cat <<EOF
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="For Logic Pro, GarageBand, Ableton Live, Bitwig and other AU hosts. Installs to /Library/Audio/Plug-Ins/Components.">
    <pkg-ref id="$ID.au"/>
  </choice>
  <choice id="vst3" title="VST3" description="For Ableton Live, Bitwig, Reaper, Cubase and other VST3 hosts. Installs to /Library/Audio/Plug-Ins/VST3.">
    <pkg-ref id="$ID.vst3"/>
  </choice>
EOF
  [[ -n "$APP" ]] && cat <<EOF
  <choice id="app" title="Standalone app" description="Play the synth without a DAW. Installs to /Applications.">
    <pkg-ref id="$ID.app"/>
  </choice>
EOF
  echo "  <pkg-ref id=\"$ID.au\">$ID.au.pkg</pkg-ref>"
  echo "  <pkg-ref id=\"$ID.vst3\">$ID.vst3.pkg</pkg-ref>"
  [[ -n "$APP" ]] && echo "  <pkg-ref id=\"$ID.app\">$ID.app.pkg</pkg-ref>"
  echo "</installer-gui-script>"
} > "$WORK/distribution.xml"

OUT="$DIST/SergeAnimalTKB-$PRODUCT-$VERSION-macOS.pkg"
rm -f "$OUT"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" \
             --resources "$WORK/res" ${SIGN_PKG:+--sign "$SIGN_PKG"} "$OUT" >/dev/null

# optional notarization (only with a Developer ID + notarytool keychain profile)
if [[ -n "$SIGN_PKG" && -n "${NOTARY_PROFILE:-}" ]]; then
  xcrun notarytool submit "$OUT" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUT"
fi

# raw bundles too, for people who prefer to copy them by hand
ZIP="$DIST/SergeAnimalTKB-$PRODUCT-$VERSION-macOS-bundles.zip"
rm -f "$ZIP"
BUNDLES="$WORK/$NAME $VERSION"
mkdir -p "$BUNDLES"
for d in "$WORK"/stage-*; do cp -R "$d"/* "$BUNDLES"/; done
ditto -c -k --keepParent "$BUNDLES" "$ZIP"

echo "wrote $OUT"
echo "wrote $ZIP"
