#!/usr/bin/env bash
# Writes the Ring Racer textures as uncompressed 32-bit TGA files (BGRA, top row first):
#   smoke.tga - 32x32 soft white puff whose alpha falls off from the centre.
#   logo.tga  - 96x32 checkered flag for the menu.
#   variation.tga - 256x256 tileable value noise in greens and dry yellows, laid thinly over the grass so that its
#                   repetition does not show.
#   plowed.tga, wheat.tga, crops.tga - 128x128 tileable fields: brown furrows, ripe wheat, green rows on soil.
set -euo pipefail
out="${1:-$(dirname "$0")/../../assets/racer}"
mkdir -p "$out"
export LC_ALL=C
# fbm(x, y, first, last): tileable value noise in [0, 1], summed over lattices of first..last cells (powers of two).
noise='function fbm(x, y, first, last,   i, n, total, weight, cells, fx, fy, ix, iy, tx, ty, a, b, c, d) {
	if (!seeded) { srand(seed); for (i = 0; i < 40000; ++i) lattice[i] = rand(); seeded = 1 }
	n = 0; total = 0; weight = 1
	for (cells = first; cells <= last; cells *= 2) {
		fx = x * cells / w; fy = y * cells / h; ix = int(fx); iy = int(fy); tx = fx - ix; ty = fy - iy
		tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
		a = lattice[cells * cells + (ix % cells) + (iy % cells) * cells]; b = lattice[cells * cells + ((ix + 1) % cells) + (iy % cells) * cells]
		c = lattice[cells * cells + (ix % cells) + ((iy + 1) % cells) * cells]; d = lattice[cells * cells + ((ix + 1) % cells) + ((iy + 1) % cells) * cells]
		n += weight * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty); total += weight; weight *= 0.5
	}
	return n / total
}
function byte(v) { return v < 0 ? 0 : v > 255 ? 255 : int(v) }'
tga() { # width height awk-pixel-program file [seed]
	awk -v w="$1" -v h="$2" -v seed="${5:-7}" "$noise BEGIN {
		printf \"%c%c%c%c%c%c%c%c%c%c%c%c\", 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0
		printf \"%c%c%c%c%c%c\", w % 256, int(w / 256), h % 256, int(h / 256), 32, 40
		for (y = 0; y < h; ++y) for (x = 0; x < w; ++x) { $3; printf \"%c%c%c%c\", b, g, r, a }
	}" > "$4"
}
tga 32 32 'd = sqrt((x - 15.5) ^ 2 + (y - 15.5) ^ 2) / 16; f = d < 1 ? (1 - d) ^ 2 : 0; r = g = b = 255; a = int(230 * f)' "$out/smoke.tga"
tga 96 32 'c = (int(x / 8) + int(y / 8)) % 2 ? 20 : 240; r = g = b = c; a = 255' "$out/logo.tga"
tga 256 256 '
	if (!seeded) { srand(7); for (i = 0; i < 4096; ++i) lattice[i] = rand(); seeded = 1 }
	n = 0; weight = 0.5
	for (cells = 4; cells <= 32; cells *= 2) {
		fx = x * cells / w; fy = y * cells / h; ix = int(fx); iy = int(fy); tx = fx - ix; ty = fy - iy
		tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
		a = lattice[cells * cells + (ix % cells) + (iy % cells) * cells]; b = lattice[cells * cells + ((ix + 1) % cells) + (iy % cells) * cells]
		c = lattice[cells * cells + (ix % cells) + ((iy + 1) % cells) * cells]; d = lattice[cells * cells + ((ix + 1) % cells) + ((iy + 1) % cells) * cells]
		n += weight * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty); weight *= 0.5
	}
	n = n / 0.9375; r = int(40 + 110 * n); g = int(75 + 70 * n); b = int(20 + 35 * n); a = 255' "$out/variation.tga"
# Fields: rows run along the texture height; each row wanders a little sideways.
tga 128 128 '
	t = x / w * 8 + 0.4 * (fbm(x, y, 2, 4) - 0.5); furrow = 0.5 + 0.5 * cos(6.2832 * t)
	k = (0.62 + 0.45 * furrow) * (0.85 + 0.3 * fbm(x, y, 16, 64)) + 0.08 * (rand() - 0.5)
	r = byte(105 * k); g = byte(78 * k); b = byte(52 * k); a = 255' "$out/plowed.tga" 12
tga 128 128 '
	k = (0.8 + 0.3 * fbm(x, y, 4, 16)) * (0.88 + 0.24 * fbm(x, y, 128, 128)) + 0.1 * (rand() - 0.5)
	r = byte(205 * k); g = byte(168 * k); b = byte(86 * k); a = 255' "$out/wheat.tga" 13
tga 128 128 '
	t = x / w * 8 + 0.3 * (fbm(x, y, 2, 4) - 0.5); t -= int(t); plant = (t - 0.5) ^ 2 < (0.24 + 0.08 * (fbm(x, y, 32, 64) - 0.5)) ^ 2
	k = (0.75 + 0.5 * fbm(x, y, 32, 128)) + 0.1 * (rand() - 0.5)
	if (plant) { r = byte(58 * k); g = byte(108 * k); b = byte(34 * k) } else { r = byte(98 * k); g = byte(74 * k); b = byte(50 * k) }
	a = 255' "$out/crops.tga" 14
