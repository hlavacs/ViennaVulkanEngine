#!/usr/bin/env sh
set -eu

platform="${1:-Mac}"
variant="${2:-Debug}"
filter="${3:-}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)

case "$platform:$variant" in
    Mac:debug)
        build_dir="$repo_root/build/vscode-debug"
        ;;
    Mac:release)
        build_dir="$repo_root/build/vscode-release"
        ;;
    Linux:debug)
        build_dir="$repo_root/build/debug-linux"
        ;;
    Linux:release)
        build_dir="$repo_root/build/release-linux"
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
    moltenvk_manifest="$build_dir/vulkan/icd.d/MoltenVK_icd.json"
    if [ -f "$moltenvk_manifest" ]; then
        export VK_ICD_FILENAMES="$moltenvk_manifest"
    fi
fi

if [ -n "$filter" ]; then
    exec ctest --test-dir "$build_dir" --output-on-failure -R "$filter"
fi

exec ctest --test-dir "$build_dir" --output-on-failure
