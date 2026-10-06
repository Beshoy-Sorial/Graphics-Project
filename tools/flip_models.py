"""
Turns the head and torso models around (180 degrees about their own vertical axis).

The body-part models come from different sources: the arms and legs (and all of the
game logic) treat local +Z as "forward", but head.obj and torso.obj had their face and
chest on the -Z side, so every fighter faced its opponent with the back of its head.

The rotation is done around the centre of each model's bounding box, so the model
keeps exactly the same footprint and no entity offsets in the configs need to change.
Rotating twice restores the original, so only run it once (already applied).

Run from the project root:
    python tools/flip_models.py
then regenerate the crowd LODs:
    python tools/generate_lods.py
"""

MODELS = ["assets/models/head.obj", "assets/models/torso.obj"]


def flip(path):
    with open(path) as f:
        lines = f.readlines()

    xs, zs = [], []
    for line in lines:
        if line.startswith("v "):
            parts = line.split()
            xs.append(float(parts[1]))
            zs.append(float(parts[3]))
    cx = (min(xs) + max(xs)) / 2.0
    cz = (min(zs) + max(zs)) / 2.0

    out = []
    for line in lines:
        if line.startswith("v "):
            p = line.split()
            x, y, z = float(p[1]), float(p[2]), float(p[3])
            rest = " " + " ".join(p[4:]) if len(p) > 4 else ""  # keep vertex colours if present
            out.append(f"v {2 * cx - x:.6f} {y:.6f} {2 * cz - z:.6f}{rest}\n")
        elif line.startswith("vn "):
            p = line.split()
            out.append(f"vn {-float(p[1]):.4f} {float(p[2]):.4f} {-float(p[3]):.4f}\n")
        else:
            out.append(line)  # faces keep their winding: a rotation is not a mirror

    with open(path, "w") as f:
        f.writelines(out)
    print(f"flipped {path} around x={cx:.3f}, z={cz:.3f}")


for model in MODELS:
    flip(model)
