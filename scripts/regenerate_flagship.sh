#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd -P)
if [ "$#" -ne 1 ]; then
    printf '%s\n' 'Usage: scripts/regenerate_flagship.sh NEW_OUTPUT_DIR' >&2
    exit 2
fi
require_clean() {
    status=$(git -C "$root" status --porcelain --untracked-files=all) &&
        test "$(git -C "$root" rev-parse --show-toplevel)" = "$root" &&
        test -z "$status"
}
if ! require_clean; then
    printf '%s\n' 'Run from a clean ARBITER-X checkout.' >&2
    exit 2
fi
producer=$(git -C "$root" rev-parse HEAD)
case "$1" in /*) output=$1 ;; *) output=$PWD/$1 ;; esac
parent=$(CDPATH= cd -- "$(dirname "$output")" && pwd -P)
case "$parent/" in "$root/"*) printf '%s\n' 'Use an output directory outside the checkout.' >&2; exit 2 ;; esac
out=$parent/$(basename "$output")
mkdir "$out"

tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
cfg=$root/configs/flagship_clustered.axexp
grep -qx 'flow_model = clustered' "$cfg"
grep -qx 'bots_observe_shadow = false' "$cfg"
cmake -S "$root" -B "$tmp/build" -DCMAKE_BUILD_TYPE=Release -DARBITER_WARNINGS_AS_ERRORS=ON
cmake --build "$tmp/build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}" --target arbiter
arbiter=$tmp/build/arbiter
"$arbiter" --version | grep -F " $producer (clean) "
for study in clustered-reactive clustered-frozen poisson-reactive; do
    case "$study" in
        clustered-reactive) cp "$cfg" "$tmp/pilot.axexp" ;;
        clustered-frozen) sed 's/^bots_observe_shadow = false$/bots_observe_shadow = true/' "$cfg" > "$tmp/pilot.axexp" ;;
        poisson-reactive) sed 's/^flow_model = clustered$/flow_model = poisson/' "$cfg" > "$tmp/pilot.axexp" ;;
    esac
    dir=$out/FLAGSHIP-$study
    "$arbiter" flagship --config "$tmp/pilot.axexp" --out "$dir"
    "$arbiter" flagship --reproduce "$dir/results.json" --report "$dir/report.md"
done
test "$(git -C "$root" rev-parse HEAD)" = "$producer"
require_clean
