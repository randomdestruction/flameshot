#!/usr/bin/env bash
# Builds the Flameshot AppImage with the ImgTaco uploader.
# Run from anywhere inside quay.io/flameshot-org/ci-building:ubuntu-noble (the
# image upstream builds its AppImage in); the result lands in the repo root.
#
# usage: imgtaco.sh [version]    e.g. imgtaco.sh 15.0.0-imgtaco.1
set -euo pipefail

src=$(cd "$(dirname "$0")/../.." && pwd)
work=$src/build-appimage
upstream_version=$(sed -n 's/^set(FLAMESHOT_VERSION \(.*\))/\1/p' "$src/CMakeLists.txt")
export VERSION=${1:-$upstream_version-imgtaco.dev}

sudo=$(command -v sudo || true)
$sudo apt-get -y -qq update
$sudo apt-get -y --no-install-recommends install fuse cmake extra-cmake-modules \
    build-essential qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
    libqt6svg6-dev qt6-l10n-tools qt6-wayland libgl-dev appstream \
    hicolor-icon-theme openssl ca-certificates jq wget file

mkdir -p "$work"
cd "$work"
for tool in linuxdeploy linuxdeploy-plugin-appimage linuxdeploy-plugin-qt; do
    wget -c -nv "https://github.com/linuxdeploy/$tool/releases/download/continuous/$tool-x86_64.AppImage"
    chmod a+x "$tool-x86_64.AppImage"
done

cmake -S "$src" -B "$work/build" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DUSE_LAUNCHER_ABSOLUTE_PATH:BOOL=OFF \
    -DENABLE_IMGTACO=ON \
    -DDISABLE_UPDATE_CHECKER=ON
cmake --build "$work/build" -j"$(nproc)"
DESTDIR="$work/AppDir" cmake --install "$work/build"
rm -rf "$work/AppDir/usr/include" "$work/AppDir/usr/lib"

export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so"
export EXTRA_QT_MODULES="waylandcompositor"
export QMAKE=/usr/lib/qt6/bin/qmake
# Qt loads OpenSSL at run time, so linuxdeploy does not pick up libssl. It has
# to match the bundled libcrypto or HTTPS fails on hosts with a newer OpenSSL.
./linuxdeploy-x86_64.AppImage --appdir "$work/AppDir" --plugin qt \
    --library /usr/lib/x86_64-linux-gnu/libssl.so.3 --output appimage

appimage=Flameshot-$VERSION-x86_64.AppImage
mv "$work/$appimage" "$src/$appimage"
sha256sum "$src/$appimage"
