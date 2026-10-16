#!/usr/bin/env bash
# Writes the two Ring Racer textures as uncompressed 32-bit TGA files (BGRA, top row first):
#   smoke.tga - 32x32 soft white puff whose alpha falls off from the centre.
#   logo.tga  - 96x32 checkered flag for the menu.
set -euo pipefail
out="${1:-$(dirname "$0")/../../assets/racer}"
mkdir -p "$out"
export LC_ALL=C
tga() { # width height awk-pixel-program file
	awk -v w="$1" -v h="$2" "BEGIN {
		printf \"%c%c%c%c%c%c%c%c%c%c%c%c\", 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0
		printf \"%c%c%c%c%c%c\", w % 256, int(w / 256), h % 256, int(h / 256), 32, 40
		for (y = 0; y < h; ++y) for (x = 0; x < w; ++x) { $3; printf \"%c%c%c%c\", b, g, r, a }
	}" > "$4"
}
tga 32 32 'd = sqrt((x - 15.5) ^ 2 + (y - 15.5) ^ 2) / 16; f = d < 1 ? (1 - d) ^ 2 : 0; r = g = b = 255; a = int(230 * f)' "$out/smoke.tga"
tga 96 32 'c = (int(x / 8) + int(y / 8)) % 2 ? 20 : 240; r = g = b = c; a = 255' "$out/logo.tga"
