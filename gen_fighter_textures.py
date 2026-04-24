"""
Run this once from the project root:
    python gen_fighter_textures.py

Creates assets/textures/fighter/*.png — tiny 1x1 solid-colour PNGs used as
albedo / specular / roughness maps for the lit fighter materials so that the
Phong lighting model runs on every character body part.
"""
from PIL import Image
import os

out = "assets/textures/fighter"
os.makedirs(out, exist_ok=True)

colours = {
    "skin":      (242, 199, 165, 255),
    "red":       (217,  38,  38, 255),
    "blue":      ( 38,  89, 217, 255),
    "white":     (230, 230, 230, 255),
    "green":     ( 25, 178,  25, 255),
    "yellow":    (204, 178,  25, 255),
    "purple":    (127,  25, 178, 255),
    "cyan":      ( 25, 178, 204, 255),
    "orange":    (229, 127,  25, 255),
    "black":     ( 51,  51,  51, 255),
    "spec_mid":  (128, 128, 128, 255),   # 50 % grey specular
    "spec_hi":   (220, 220, 220, 255),   # bright specular (referee white)
    "rough_mid": (128, 128, 128, 255),   # medium roughness
    "rough_lo":  ( 64,  64,  64, 255),   # low roughness (shinier)
}

for name, rgba in colours.items():
    path = os.path.join(out, f"{name}.png")
    Image.new("RGBA", (1, 1), rgba).save(path)
    print(f"  created {path}")

print("Done! All fighter textures generated.")
