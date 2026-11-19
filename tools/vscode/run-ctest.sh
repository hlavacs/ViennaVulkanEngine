#!/usr/bin/env sh
set -eu

platform="${1:-Mac}"
variant="${2:-debug}"
filter="${3:-}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)

case "$platform:$variant" in
    Mac:debug|Mac:release)
        build_dir="$repo_root/build/complex-$variant-macos"
        ;;
    Linux:debug|Linux:release)
        build_dir="$repo_root/build/complex-$variant-linux"
        ;;
    Windows:*)
        echo "The Windows test launcher must be run on Windows." >&2
        exit 2
        ;;
    *)
        echo "Unsupported platform/variant combination: $platform / $variant" >&2
        exit 2
        ;;
esac

if [ "$platform" = "Mac" ]; then
    kosmickrisp_manifest="$build_dir/vulkan/icd.d/libkosmickrisp_icd.json"
    if [ -f "$kosmickrisp_manifest" ]; then
        export VK_ICD_FILENAMES="$kosmickrisp_manifest"
    fi
fi

if [ -n "$filter" ]; then
    exec ctest --test-dir "$build_dir" --output-on-failure -R "$filter"
fi

exec ctest --test-dir "$build_dir" --output-on-failure
