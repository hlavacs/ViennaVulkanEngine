#!/usr/bin/env python3
"""Turns the Poly Haven texture downloads (CC0, 1k JPG) into the Ring Racer textures in assets/racer/textures.

Usage: python prepare_textures.py <download folder> [<output folder>]   (needs Pillow: pip install pillow)

Each texture is levelled to a target mean brightness, partly desaturated and, where it is blotchy, flattened in contrast,
so that the game can tint it with a material colour (red and white kerbs from one concrete, a red barn from grey
planks), then saved as a smaller JPG.
The download links are listed in examples/racer/README.md.
"""
import sys
from pathlib import Path

from PIL import Image, ImageEnhance, ImageStat

# Output name: (Poly Haven file, target mean brightness 0..255 or None to keep, saturation factor, JPG quality, contrast factor).
TEXTURES = {
    "asphalt_track.jpg": ("asphalt_track_diff_1k.jpg", 82, 0.5, 82),
    "leafy_grass.jpg": ("leafy_grass_diff_1k.jpg", None, 1.0, 82),
    "leafy_grass_normal.jpg": ("leafy_grass_nor_gl_1k.jpg", None, 1.0, 92),
    "concrete_wall.jpg": ("concrete_wall_008_diff_1k.jpg", 175, 0.5, 82),
    "concrete_kerb.jpg": ("concrete_floor_01_diff_1k.jpg", 215, 0.0, 82),
    "concrete_floor.jpg": ("concrete_floor_02_diff_1k.jpg", 165, 0.6, 82),
    "weathered_planks.jpg": ("weathered_planks_diff_1k.jpg", 150, 0.3, 82),
    "white_plaster.jpg": ("white_plaster_02_diff_1k.jpg", 215, 0.5, 82, 0.5),
    "clay_roof_tiles.jpg": ("clay_roof_tiles_02_diff_1k.jpg", None, 1.0, 82),
    "corrugated_iron.jpg": ("corrugated_iron_diff_1k.jpg", 170, 0.6, 82),
}


def level(image: Image.Image, target: float | None, saturation: float, contrast: float = 1.0) -> Image.Image:
    """Reduces the colour and the contrast, then scales the brightness so that the mean grey value reaches the target."""
    image = ImageEnhance.Contrast(ImageEnhance.Color(image).enhance(saturation)).enhance(contrast)
    if target is not None:
        mean = sum(ImageStat.Stat(image.convert("L")).mean)
        image = ImageEnhance.Brightness(image).enhance(target / max(mean, 1.0))
    return image


def main() -> None:
    source = Path(sys.argv[1])
    target = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).resolve().parents[2] / "assets/racer/textures"
    target.mkdir(parents=True, exist_ok=True)
    for name, (download, brightness, saturation, quality, *contrast) in TEXTURES.items():
        image = Image.open(source / download).convert("RGB").resize((1024, 1024), Image.LANCZOS)
        level(image, brightness, saturation, *contrast).save(target / name, quality=quality, optimize=True)
        print(f"{name}: from {download}")


if __name__ == "__main__":
    main()
