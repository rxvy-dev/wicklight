#!/usr/bin/env bash
# Publish WICKLIGHT to GitHub (github.com/rxvy-dev/wicklight by default).
#   ./publish.sh                 create the repo if needed, commit, push
#   ./publish.sh --release       ...and also build dist/ and upload a GitHub release (tarball + AppImage)
#   ./publish.sh --private       create the repo as private
# Env overrides: GH_USER=rxvy-dev REPO=wicklight VISIBILITY=public MSG="commit message"
# Needs: git, and the GitHub CLI (gh) logged in:  gh auth login
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

GH_USER="${GH_USER:-rxvy-dev}"
REPO="${REPO:-wicklight}"
VISIBILITY="${VISIBILITY:-public}"
VERSION="$(grep -m1 '^VERSION=' build.sh | cut -d= -f2)"
DO_RELEASE=0
for a in "$@"; do
    case "$a" in
        --release) DO_RELEASE=1 ;;
        --private) VISIBILITY=private ;;
        --public)  VISIBILITY=public ;;
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "unknown option $a"; exit 1 ;;
    esac
done

say() { printf '\033[1;33m[publish]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[publish] error:\033[0m %s\n' "$*" >&2; exit 1; }

command -v git >/dev/null || die "git is not installed"
command -v gh  >/dev/null || die "GitHub CLI 'gh' is not installed (Arch: github-cli, Debian/Ubuntu/Fedora: gh, Void: github-cli)"
gh auth status >/dev/null 2>&1 || die "not logged in to GitHub - run: gh auth login"

if [[ ! -d .git ]]; then
    git init -q
    git branch -M main
    say "initialised git repository"
fi
git add -A
if ! git diff --cached --quiet; then
    git commit -q -m "${MSG:-Wicklight $VERSION}"
    say "committed changes"
else
    say "nothing new to commit"
fi

if ! gh repo view "$GH_USER/$REPO" >/dev/null 2>&1; then
    say "creating $VISIBILITY repo github.com/$GH_USER/$REPO ..."
    gh repo create "$GH_USER/$REPO" "--$VISIBILITY" \
        --description "WICKLIGHT - a native Linux 3D pixel action-adventure. Your head is the last flame in the world. Throw it." \
        --homepage "https://github.com/$GH_USER/$REPO"
fi
git remote get-url origin >/dev/null 2>&1 || git remote add origin "https://github.com/$GH_USER/$REPO.git"
git push -u origin main
say "pushed to https://github.com/$GH_USER/$REPO"

if [[ $DO_RELEASE == 1 ]]; then
    ./build.sh release
    files=(dist/*.tar.gz)
    ls dist/*.AppImage >/dev/null 2>&1 && files+=(dist/*.AppImage)
    tag="v$VERSION"
    if gh release view "$tag" -R "$GH_USER/$REPO" >/dev/null 2>&1; then
        say "release $tag exists - uploading assets (overwriting)"
        gh release upload "$tag" "${files[@]}" --clobber -R "$GH_USER/$REPO"
    else
        gh release create "$tag" "${files[@]}" -R "$GH_USER/$REPO" --title "Wicklight $VERSION" \
            --notes "Native Linux build (x86_64). Download the AppImage and run it, or extract the tar.gz and run ./wicklight. Needs OpenGL 3.3; SDL2 is used from your system (a copy is bundled as a fallback)."
    fi
    say "release: https://github.com/$GH_USER/$REPO/releases/tag/$tag"
fi
