"""
Generates low-poly LOD (level-of-detail) versions of the fighter body-part meshes.

The original torso / leg / arm models are very dense (the torso alone has ~127K
vertices). That is fine for the two fighters, but the procedurally generated
crowd instantiates them ~150 times, which pushed more than 40 million vertices
per frame through the GPU. The crowd is always far from the camera, so it uses
these decimated copies instead (same shape and coordinate frame).

Run once from the project root (requires: pip install pyfqmr numpy):
    python tools/generate_lods.py
"""
import os
import numpy as np
import pyfqmr

MODELS = "assets/models"
TARGETS = {          # mesh name -> target triangle count
    "torso":     3000,
    "left_leg":  1200,
    "right_leg": 1200,
    "left_arm":  900,
    "right_arm": 900,
}


def load_obj(path):
    verts, faces = [], []
    with open(path) as f:
        for line in f:
            if line.startswith("v "):
                verts.append([float(x) for x in line.split()[1:4]])
            elif line.startswith("f "):
                idx = [int(tok.split("/")[0]) for tok in line.split()[1:]]
                idx = [i - 1 if i > 0 else len(verts) + i for i in idx]
                for k in range(1, len(idx) - 1):  # fan-triangulate polygons
                    faces.append([idx[0], idx[k], idx[k + 1]])
    return np.array(verts, dtype=np.float64), np.array(faces, dtype=np.int32)


def smooth_normals(verts, faces):
    normals = np.zeros_like(verts)
    tri = verts[faces]
    face_n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])  # area weighted
    for k in range(3):
        np.add.at(normals, faces[:, k], face_n)
    length = np.linalg.norm(normals, axis=1, keepdims=True)
    return normals / np.maximum(length, 1e-12)


def save_obj(path, verts, normals, faces, source):
    with open(path, "w") as f:
        f.write(f"# LOD generated from {source} by tools/generate_lods.py\n")
        for v in verts:
            f.write(f"v {v[0]:.5f} {v[1]:.5f} {v[2]:.5f}\n")
        for n in normals:
            f.write(f"vn {n[0]:.4f} {n[1]:.4f} {n[2]:.4f}\n")
        for a, b, c in faces + 1:
            f.write(f"f {a}//{a} {b}//{b} {c}//{c}\n")


for name, target in TARGETS.items():
    src = os.path.join(MODELS, f"{name}.obj")
    verts, faces = load_obj(src)
    simplifier = pyfqmr.Simplify()
    simplifier.setMesh(verts, faces)
    simplifier.simplify_mesh(target_count=target, aggressiveness=5, preserve_border=True, verbose=False)
    v2, f2, _ = simplifier.getMesh()
    dst = os.path.join(MODELS, f"{name}_lod.obj")
    save_obj(dst, v2, smooth_normals(v2, f2), f2, f"{name}.obj")
    print(f"{name:10s}: {len(faces):7d} -> {len(f2):5d} triangles  ({dst})")
