#!/usr/bin/env bash
# Rebuild both PDF guides while keeping temporary TeX files outside the source tree.
set -euo pipefail
cd "$(dirname "$0")"
guide_build_dir="$(mktemp -d "${TMPDIR:-/tmp}/vve-guides.XXXXXX")"
for guide_name in vvengine_beginner_quickstart simple_engine_explained; do
    # Later passes resolve references after the contents has changed pagination.
    for guide_pass in 1 2 3; do
        if ! xelatex -interaction=nonstopmode -halt-on-error -file-line-error \
            -output-directory="$guide_build_dir" "$guide_name.tex" \
            > "$guide_build_dir/$guide_name.pass$guide_pass.txt"; then
            tail -n 25 "$guide_build_dir/$guide_name.pass$guide_pass.txt" >&2
            printf 'Guide build failed. Logs: %s\n' "$guide_build_dir" >&2
            exit 1
        fi
    done
    cp "$guide_build_dir/$guide_name.pdf" "$guide_name.pdf"
done
printf 'Updated both guides. Typesetting logs: %s\n' "$guide_build_dir"
