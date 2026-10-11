#!/usr/bin/env bash
# WICKLIGHT build script
#   ./build.sh               build build/wicklight with your system compiler
#   ./build.sh run [args]    build, then play
#   ./build.sh test          build, then run the headless full-campaign autoplay test
#   ./build.sh release       portable build -> dist/ (folder, .tar.gz, and AppImage when possible)
#   ./build.sh install       install to ~/.local (command, menu entry, icon)
#   ./build.sh uninstall     remove the ~/.local install
#   ./build.sh clean         delete build/ and dist/
# Env: CXX=clang++ to pick a compiler, JOBS=N for parallel compiles.
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

APP=wicklight
VERSION=1.1.0
BUILD=build
DIST=dist
CXXFLAGS_COMMON=(-std=c++17 -O2 -Wall -Wno-unused-parameter -Wno-unused-result -Isrc -Ithird_party/include)

say()  { printf '\033[1;33m[wicklight]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[wicklight] error:\033[0m %s\n' "$*" >&2; exit 1; }

find_cxx() {
    if [[ -n "${CXX:-}" ]]; then echo "$CXX"; return; fi
    for c in g++ clang++ c++; do command -v "$c" >/dev/null 2>&1 && { echo "$c"; return; }; done
    die "no C++ compiler found. Install one:
  Arch:          sudo pacman -S base-devel
  Debian/Ubuntu: sudo apt install build-essential
  Fedora:        sudo dnf install gcc-c++
  openSUSE:      sudo zypper install gcc-c++
  Void:          sudo xbps-install base-devel
  Gentoo:        (gcc is already installed)
  NixOS:         nix develop   (or: nix build)"
}

compile() {  # $1 = compiler (may contain spaces), $2 = output, rest = extra flags
    local cxx="$1" out="$2"; shift 2
    mkdir -p "$(dirname "$out")" "$BUILD/obj"
    local jobs="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
    local objs=() pids=() n=0
    for src in src/*.cpp; do
        local obj="$BUILD/obj/$(basename "${src%.cpp}").$(echo "$cxx $*" | md5sum | cut -c1-8).o"
        objs+=("$obj")
        if [[ ! -f "$obj" || "$src" -nt "$obj" || -n "$(find src -name '*.h' -newer "$obj" 2>/dev/null)" ]]; then
            $cxx "${CXXFLAGS_COMMON[@]}" "$@" -c "$src" -o "$obj" &
            pids+=($!); n=$((n+1))
            if (( n % jobs == 0 )); then for p in "${pids[@]}"; do wait "$p" || die "compile failed"; done; pids=(); fi
        fi
    done
    for p in "${pids[@]}"; do wait "$p" || die "compile failed"; done
    $cxx "${objs[@]}" "$@" -ldl -o "$out" || die "link failed"
}

build() {
    local cxx; cxx="$(find_cxx)"
    say "compiling with $cxx ..."
    compile "$cxx" "$BUILD/$APP"
    say "built $BUILD/$APP"
}

check_runtime() {
    if ! ldconfig -p 2>/dev/null | grep -q 'libSDL2-2.0.so.0' && [[ ! -f "$DIST/$APP-linux-x86_64/lib/libSDL2-2.0.so.0" ]]; then
        say "note: SDL2 runtime not found by ldconfig. If the game says SDL2 is missing, install it:"
        say "  Arch: sdl2 | Debian/Ubuntu: libsdl2-2.0-0 | Fedora: SDL2 | openSUSE: libSDL2-2_0-0 | Void: SDL2 | Gentoo: media-libs/libsdl2"
    fi
}

release() {
    local out="$DIST/$APP-linux-x86_64"
    rm -rf "$out"; mkdir -p "$out/bin" "$out/lib"
    local zig=""
    if command -v zig >/dev/null 2>&1; then zig="zig c++";
    elif python3 -c 'import ziglang' >/dev/null 2>&1; then zig="python3 -m ziglang c++"; fi
    if [[ -n "$zig" ]]; then
        say "portable build with zig (targets glibc 2.17, static libc++) ..."
        compile "$zig -target x86_64-linux-gnu.2.17" "$out/bin/$APP" -s
    else
        local cxx; cxx="$(find_cxx)"
        say "zig not found; building with $cxx and a static libstdc++ (install zig, or 'pip install ziglang', for the most portable binary)"
        compile "$cxx" "$out/bin/$APP" -static-libstdc++ -static-libgcc -s
    fi
    # bundle SDL2 as a fallback for systems without it (the system copy is preferred at runtime)
    local sdl
    sdl="$(ldconfig -p 2>/dev/null | awk '/libSDL2-2.0.so.0 .*x86-64/ {print $NF; exit}')"
    if [[ -n "$sdl" && -f "$sdl" ]]; then cp -L "$sdl" "$out/lib/libSDL2-2.0.so.0"; say "bundled fallback $(basename "$sdl")"; fi
    cat > "$out/$APP" <<'EOS'
#!/bin/sh
# WICKLIGHT launcher - works from any directory, no root needed.
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/bin/wicklight" "$@"
EOS
    chmod +x "$out/$APP"
    cp README.md LICENSE packaging/wicklight.png packaging/wicklight.desktop "$out/"
    tar -C "$DIST" -czf "$DIST/$APP-$VERSION-linux-x86_64.tar.gz" "$APP-linux-x86_64"
    say "release folder:  $out"
    say "release archive: $DIST/$APP-$VERSION-linux-x86_64.tar.gz"
    appimage "$out"
}

appimage() {
    local src="$1" tool=""
    if command -v appimagetool >/dev/null 2>&1; then tool="appimagetool";
    else
        mkdir -p .tools
        if [[ ! -x .tools/appimagetool ]]; then
            say "downloading appimagetool ..."
            curl -fsSL -o .tools/appimagetool https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage 2>/dev/null \
                && chmod +x .tools/appimagetool || { rm -f .tools/appimagetool; say "could not download appimagetool - skipping AppImage (tarball is ready)"; return 0; }
        fi
        tool=".tools/appimagetool"
    fi
    local appdir="$BUILD/AppDir"
    rm -rf "$appdir"; mkdir -p "$appdir/usr/bin" "$appdir/usr/lib"
    cp "$src/bin/$APP" "$appdir/usr/bin/"
    [[ -f "$src/lib/libSDL2-2.0.so.0" ]] && cp "$src/lib/libSDL2-2.0.so.0" "$appdir/usr/lib/"
    cp packaging/wicklight.desktop packaging/wicklight.png "$appdir/"
    cat > "$appdir/AppRun" <<'EOS'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/wicklight" "$@"
EOS
    chmod +x "$appdir/AppRun"
    rm -f "$DIST/Wicklight-$VERSION-x86_64.AppImage"
    { ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --no-appstream "$appdir" "$DIST/Wicklight-$VERSION-x86_64.AppImage" >/dev/null 2>&1 \
      || ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --no-appstream "$appdir" "$DIST/Wicklight-$VERSION-x86_64.AppImage" >"$BUILD/appimagetool.log" 2>&1; } \
        && say "AppImage: $DIST/Wicklight-$VERSION-x86_64.AppImage" \
        || say "appimagetool failed (see $BUILD/appimagetool.log) - skipping AppImage (tarball is ready)"
}

install_local() {
    [[ -x "$BUILD/$APP" ]] || build
    local prefix="${PREFIX:-$HOME/.local}"
    mkdir -p "$prefix/share/$APP" "$prefix/bin" "$prefix/share/applications" "$prefix/share/icons/hicolor/256x256/apps"
    cp "$BUILD/$APP" "$prefix/share/$APP/$APP"
    ln -sf "$prefix/share/$APP/$APP" "$prefix/bin/$APP"
    cp packaging/wicklight.png "$prefix/share/icons/hicolor/256x256/apps/"
    cp packaging/wicklight.png "$prefix/share/$APP/wicklight.png"
    # absolute paths: app launchers don't read your shell's PATH, so a bare "Exec=wicklight" never starts
    sed -e "s|^Exec=.*|Exec=\"$prefix/share/$APP/$APP\"|" \
        -e "s|^Icon=.*|Icon=$prefix/share/$APP/wicklight.png|" \
        packaging/wicklight.desktop > "$prefix/share/applications/wicklight.desktop"
    chmod +x "$prefix/share/applications/wicklight.desktop"
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$prefix/share/applications" >/dev/null 2>&1 || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$prefix/share/icons/hicolor" >/dev/null 2>&1 || true
    say "installed: Wicklight is now in your app launcher (and the '$APP' command: $prefix/bin/$APP)"
}

uninstall_local() {
    local prefix="${PREFIX:-$HOME/.local}"
    rm -rf "$prefix/share/$APP" "$prefix/bin/$APP" "$prefix/share/applications/wicklight.desktop" "$prefix/share/icons/hicolor/256x256/apps/wicklight.png"
    say "removed (your saves in ~/.local/share/wicklight were kept)"
}

cmd="${1:-build}"; shift || true
case "$cmd" in
    build)     build ;;
    run)       build; check_runtime; exec "$BUILD/$APP" "$@" ;;
    test)      build
               say "running the full-campaign autoplay test (headless) ..."
               WICKLIGHT_DATA_DIR="$(mktemp -d)" "$BUILD/$APP" --headless --autotest
               say "boss fights with real damage + gate-skip check ..."
               for t in bellwether warden matron ratking hush gatewarp robots; do WICKLIGHT_DATA_DIR="$(mktemp -d)" "$BUILD/$APP" --headless --autotest "$t" | grep -E 'real damage|warp past|wall|robot' || exit 1; done
               say "Endless Dark: playing generated depths 1-30 back to back (and 1-8 without warp) ..."
               WICKLIGHT_DATA_DIR="$(mktemp -d)" "$BUILD/$APP" --headless --autotest endless | grep -E 'depth|endless:'; [ "${PIPESTATUS[0]}" -eq 0 ] || { say "endless test FAILED"; exit 1; }
               WICKLIGHT_DATA_DIR="$(mktemp -d)" "$BUILD/$APP" --headless --autotest endless-nowarp | grep -E 'endless:' || exit 1
               say "new content: hacker puzzles + GL1TCH, the three new powers, Lumen City, side jobs / fishing / race / hats ..."
               for t in hacker powers city extras postgame fun; do WICKLIGHT_DATA_DIR="$(mktemp -d)" "$BUILD/$APP" --headless --autotest "$t" | grep -E "^\[bot\] $t|powers:|extras:|hacker:|postgame:|fun:" ; [ "${PIPESTATUS[0]}" -eq 0 ] || { say "$t test FAILED"; exit 1; }; done
               say "Wick's Island block world test (mine, place, craft, doors, beds, chests, ladders, bombs, night, saves) ..."
               WICKLIGHT_DATA_DIR="$(mktemp -d)" "$BUILD/$APP" --headless --autotest island | grep -E 'island'; [ "${PIPESTATUS[0]}" -eq 0 ] || { say "island test FAILED"; exit 1; } ;;
    release)   release ;;
    install)   install_local ;;
    uninstall) uninstall_local ;;
    clean)     rm -rf "$BUILD" "$DIST"; say "cleaned" ;;
    -h|--help|help) sed -n '2,12p' "$0" ;;
    *) die "unknown command '$cmd' (try ./build.sh help)" ;;
esac
