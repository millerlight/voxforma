#!/usr/bin/env bash
#
#  apply-juce-patches.sh
#
#  Applies the MinGW compatibility patches to the JUCE submodule.
#
#  The patches adapt a handful of JUCE sources so that the framework can be
#  compiled for Windows x86_64 with MinGW-w64 GCC. Upstream JUCE only
#  supports MSVC on Windows, so without these patches the cross-compile
#  fails with a hard "#error" in juce_TargetPlatform.h.
#
#  The patched files are marked with "git update-index --skip-worktree" so that
#  the parent repository does not report the submodule as modified.
#
#  Usage:
#      ./patches/apply-juce-patches.sh            apply all patches
#      ./patches/apply-juce-patches.sh --check    only report the state
#      ./patches/apply-juce-patches.sh --reverse  undo all patches
#
#  Run this once after cloning, before the first build:
#      git submodule update --init --recursive
#      ./patches/apply-juce-patches.sh
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PATCH_DIR="${SCRIPT_DIR}/juce-mingw"
JUCE_DIR="${REPO_ROOT}/JUCE"

MODE="apply"

case "${1:-}" in
    --check)   MODE="check"   ;;
    --reverse) MODE="reverse" ;;
    "")        MODE="apply"   ;;
    *)         echo "Usage: $0 [--check|--reverse]" >&2; exit 1 ;;
esac

if [ ! -d "${JUCE_DIR}" ]; then
    echo "FEHLER: JUCE-Submodul nicht gefunden unter ${JUCE_DIR}" >&2
    echo "       Bitte zuerst ausführen: git submodule update --init --recursive" >&2
    exit 1
fi

if [ ! -d "${JUCE_DIR}/.git" ] && [ ! -f "${JUCE_DIR}/.git" ]; then
    echo "FEHLER: ${JUCE_DIR} ist kein Git-Checkout." >&2
    exit 1
fi

mapfile -t PATCHES < <(find "${PATCH_DIR}" -maxdepth 1 -name '*.patch' -type f | sort)

if [ ${#PATCHES[@]} -eq 0 ]; then
    echo "FEHLER: keine Patch-Dateien in ${PATCH_DIR}" >&2
    exit 1
fi

# The files that the patches touch. They are collected up front so that the
# skip-worktree flags can be set and cleared as a whole.
TOUCHED=()
for patch in "${PATCHES[@]}"; do
    while IFS= read -r file; do
        TOUCHED+=("${file}")
    done < <(git -C "${JUCE_DIR}" apply --numstat "${patch}" 2>/dev/null | awk '{print $3}')
done

if [ ${#TOUCHED[@]} -eq 0 ]; then
    echo "FEHLER: die Patch-Dateien berühren keine bekannten Dateien." >&2
    exit 1
fi

set_skip_worktree() {
    git -C "${JUCE_DIR}" update-index --skip-worktree "${TOUCHED[@]}"
}

clear_skip_worktree() {
    git -C "${JUCE_DIR}" update-index --no-skip-worktree "${TOUCHED[@]}" 2>/dev/null || true
}

echo "=== JUCE MinGW-Patches ==="
echo "JUCE:   ${JUCE_DIR}"
echo "Commit: $(git -C "${JUCE_DIR}" rev-parse --short HEAD)"
echo "Modus:  ${MODE}"
echo

report_state() {
    local all_applied=1
    local none_applied=1

    for patch in "${PATCHES[@]}"; do
        local name
        name="$(basename "${patch}")"

        if git -C "${JUCE_DIR}" apply --reverse --check "${patch}" >/dev/null 2>&1; then
            echo "  [angewendet] ${name}"
            none_applied=0
        elif git -C "${JUCE_DIR}" apply --check "${patch}" >/dev/null 2>&1; then
            echo "  [offen     ] ${name}"
            all_applied=0
        else
            echo "  [konflikt  ] ${name}"
            all_applied=0
            none_applied=0
        fi
    done

    echo
    if [ ${all_applied} -eq 1 ]; then
        echo "Alle Patches sind angewendet."
    elif [ ${none_applied} -eq 1 ]; then
        echo "Keine Patches sind angewendet."
    else
        echo "Teilweise angewendet."
    fi
}

if [ "${MODE}" = "check" ]; then
    report_state
    exit 0
fi

if [ "${MODE}" = "reverse" ]; then
    clear_skip_worktree

    for patch in "${PATCHES[@]}"; do
        if git -C "${JUCE_DIR}" apply --reverse --check "${patch}" >/dev/null 2>&1; then
            git -C "${JUCE_DIR}" apply --reverse "${patch}"
            echo "  [entfernt ] $(basename "${patch}")"
        fi
    done

    echo
    echo "Patches zurückgenommen."
    exit 0
fi

for patch in "${PATCHES[@]}"; do
    name="$(basename "${patch}")"

    if git -C "${JUCE_DIR}" apply --reverse --check "${patch}" >/dev/null 2>&1; then
        echo "  [bereits an] ${name}"
        continue
    fi

    if ! git -C "${JUCE_DIR}" apply --check "${patch}" >/dev/null 2>&1; then
        echo "  FEHLER: ${name} lässt sich weder anwenden noch zurücknehmen." >&2
        echo "         Vermutlich ein Konflikt mit einer lokalen Änderung in JUCE." >&2
        echo "         Rücksetzen mit: git -C JUCE checkout -- ." >&2
        exit 1
    fi

    git -C "${JUCE_DIR}" apply "${patch}"
    echo "  [angewendet] ${name}"
done

set_skip_worktree

echo
echo "Patches angewendet. JUCE ist jetzt MinGW-fähig."
