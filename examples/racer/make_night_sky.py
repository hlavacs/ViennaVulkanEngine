#!/usr/bin/env python3
"""Writes the Ring Racer night sky assets/racer/night_sky.jpg from the day panorama assets/racer/sky.jpg.

Usage: python make_night_sky.py [<day panorama> [<night panorama>]]   (needs Pillow and NumPy)

The clouds of the day photo (white where the blue sky is not) become dark grey night clouds; near the moon their edges
catch some light. The moon sits where the sun is in the day photo (u 0.59, v 0.24), so the game's directional light,
which is the sun by day and the moon by night, keeps its direction. Stars shine only through the clear sky, and the lower
half fades into a dark haze.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[2]


def smoothstep(low: float, high: float, x: np.ndarray) -> np.ndarray:
    t = np.clip((x - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def main() -> None:
    day_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets/racer/sky.jpg"
    night_path = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "assets/racer/night_sky.jpg"
    day = np.asarray(Image.open(day_path).convert("RGB")).astype(np.float32) / 255.0
    height, width, _ = day.shape
    v = (np.arange(height, dtype=np.float32) + 0.5)[:, None] / height        # 0 at the zenith, 0.5 at the horizon
    u = (np.arange(width, dtype=np.float32) + 0.5)[None, :] / width
    elevation = np.clip(1.0 - 2.0 * v, -1.0, 1.0)                             # 1 at the zenith, 0 at the horizon

    # Clouds: the day sky is blue (red well below blue), clouds are white or grey. Shade keeps their structure.
    red, blue = day[..., 0], day[..., 2]
    cloud = smoothstep(0.66, 0.9, red / np.maximum(blue, 1e-3)) * smoothstep(-0.02, 0.06, elevation)
    shade = day.mean(axis=2)

    # Sky: deep blue at the zenith, a little lighter at the horizon.
    zenith, horizon = np.array([5, 7, 17], np.float32), np.array([26, 30, 46], np.float32)
    mix = np.clip(elevation, 0.0, 1.0)[..., None] ** 0.5
    sky = np.broadcast_to(horizon * (1.0 - mix) + zenith * mix, (height, width, 3)).copy()

    # Moon where the sun is, with a soft glow around it.
    du = np.minimum(np.abs(u - 0.59), 1.0 - np.abs(u - 0.59)) * width
    dv = (v - 0.24) * height
    distance = np.sqrt((du * np.cos((0.5 - v) * np.pi)) ** 2 + dv ** 2)       # Pixels, corrected for the latitude.
    glow = np.exp(-distance / 260.0)[..., None]
    sky += glow * np.array([38, 42, 52], np.float32)

    # Stars, denser towards the zenith, only where the sky is clear.
    rng = np.random.default_rng(7)
    count = 2600
    stars_v = np.arccos(rng.uniform(0.05, 1.0, count)) / np.pi             # Uniform over the upper hemisphere.
    stars_u = rng.uniform(0.0, 1.0, count)
    brightness = rng.uniform(60, 255, count) * rng.uniform(0.3, 1.0, count)
    for sv, su, b in zip(stars_v, stars_u, brightness):
        y, x = int(sv * height), int(su * width)
        if cloud[y, x] < 0.15:
            sky[y, x] += b * (1.0 - cloud[y, x] / 0.15) * np.array([0.95, 0.97, 1.0], np.float32)

    # Dark clouds: grey with the photo's shading, lighter rims where they face the moon glow.
    edges = np.asarray(Image.fromarray((cloud * 255).astype(np.uint8)).filter(ImageFilter.FIND_EDGES), np.float32) / 255.0
    rim = np.asarray(Image.fromarray((edges * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3)), np.float32)[..., None] / 255.0
    cloud_color = np.array([16, 18, 25], np.float32) * (0.55 + 0.9 * shade[..., None]) + glow * (70.0 + 400.0 * rim) * np.array([0.8, 0.85, 1.0])
    night = sky * (1.0 - cloud[..., None]) + cloud_color * cloud[..., None]

    # The disc of the moon in front of everything, with a soft edge.
    disc = smoothstep(16.0, 13.0, distance)[..., None]
    night = night * (1.0 - disc) + np.array([236, 234, 220], np.float32) * disc

    # Below the horizon: a dark haze.
    below = smoothstep(0.0, 0.12, -elevation)[..., None]
    night = night * (1.0 - below) + np.array([10, 12, 16], np.float32) * below
    Image.fromarray(np.clip(night, 0, 255).astype(np.uint8)).save(night_path, quality=85, optimize=True)
    print(f"{night_path}: {width}x{height}")


if __name__ == "__main__":
    main()
