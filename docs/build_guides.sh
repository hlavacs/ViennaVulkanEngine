#!/usr/bin/env bash
# Rebuild both PDF guides while keeping temporary TeX files outside the source tree.
set -euo pipefail
cd "$(dirname "$0")"
guide_build_dir="$(mktemp -d "${TMPDIR:-/tmp}/vve-guides.XXXXXX")"
for guide_name in vvengine_beginner_quickstart simple_engine_explained; do
    # A second pass resolves contents, page links, and cross-references.
    for guide_pass in 1 2; do
        xelatex -interaction=nonstopmode -halt-on-error -file-line-error \
            -output-directory="$guide_build_dir" "$guide_name.tex" \
            > "$guide_build_dir/$guide_name.pass$guide_pass.txt"
    done
    cp "$guide_build_dir/$guide_name.pdf" "$guide_name.pdf"
done
printf 'Updated both guides. Typesetting logs: %s\n' "$guide_build_dir"
