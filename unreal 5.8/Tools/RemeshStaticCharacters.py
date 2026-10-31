"""Remesh the four dense, static approved sculptures with installed Instant Meshes.

Run with Blender's bundled Python (NumPy), passing --instant-meshes <exe>.
Original source files and the artist's open applications are never modified.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import time

import numpy as np

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
EXPORTS = ROOT / "assets/instant_meshes"
SAVED = PROJECT / "Saved/StaticRemesh"
GOALS = {"dagon": 30000, "cthulhu_avatar": 40000, "nyarlathotep": 26000, "yog_sothoth": 40000}


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def read_ply(path):
    with path.open("rb") as f:
        header = []
        while True:
            line = f.readline().decode("ascii").strip()
            header.append(line)
            assert len(header) < 100
            if line == "end_header":
                break
        assert "format binary_little_endian 1.0" in header
        nv = int(next(s.split()[-1] for s in header if s.startswith("element vertex ")))
        nf = int(next(s.split()[-1] for s in header if s.startswith("element face ")))
        vertices = np.fromfile(f, dtype="<f4", count=nv * 3).reshape(nv, 3)
        faces = np.fromfile(f, dtype=np.dtype([("n", "u1"), ("v", "<u4", (3,))]), count=nf)
        assert len(faces) == nf and np.all(faces["n"] == 3) and f.read(1) == b""
        assert np.isfinite(vertices).all() and int(faces["v"].max()) < nv
        return vertices, faces["v"].copy()


def welded_input(source, target):
    vertices, triangles = read_ply(source)
    # Remove only exact coincident coordinates introduced by GLB UV/normal
    # seams. No smoothing, quantization or displacement of the source surface.
    unique, inverse = np.unique(vertices, axis=0, return_inverse=True)
    faces = inverse[triangles].astype("<u4")
    valid = (faces[:, 0] != faces[:, 1]) & (faces[:, 0] != faces[:, 2]) & (faces[:, 1] != faces[:, 2])
    faces = faces[valid]
    header = f"ply\nformat binary_little_endian 1.0\nelement vertex {len(unique)}\nproperty float x\nproperty float y\nproperty float z\nelement face {len(faces)}\nproperty list uchar uint vertex_indices\nend_header\n"
    packed = np.empty(len(faces), dtype=np.dtype([("n", "u1"), ("v", "<u4", (3,))]))
    packed["n"] = 3
    packed["v"] = faces
    with target.open("wb") as f:
        f.write(header.encode("ascii"))
        unique.astype("<f4").tofile(f)
        packed.tofile(f)
    assert np.array_equal(vertices.min(axis=0), unique.min(axis=0))
    assert np.array_equal(vertices.max(axis=0), unique.max(axis=0))
    return {"source_vertices": len(vertices), "welded_vertices": len(unique),
            "source_triangles": len(triangles), "input_triangles": len(faces),
            "zero_index_triangles_removed": int(len(triangles) - len(faces)),
            "source_bounds": [*vertices.min(axis=0).tolist(), *vertices.max(axis=0).tolist()]}


def inspect_obj(path):
    vertices, faces = [], []
    with path.open(encoding="utf-8") as f:
        for line in f:
            if line.startswith("v "):
                vertices.append([float(x) for x in line.split()[1:4]])
            elif line.startswith("f "):
                face = [int(x.split("/")[0]) - 1 for x in line.split()[1:]]
                assert len(face) >= 3 and len(set(face)) == len(face)
                faces.append(face)
    v = np.array(vertices)
    assert len(v) and faces and np.isfinite(v).all()
    assert all(min(f) >= 0 and max(f) < len(v) for f in faces)
    quads = sum(len(f) == 4 for f in faces)
    return {"vertices": len(v), "faces": len(faces), "quads": quads,
            "triangles_after_triangulation": sum(len(f) - 2 for f in faces),
            "bounds": [*v.min(axis=0).tolist(), *v.max(axis=0).tolist()],
            "bytes": path.stat().st_size, "sha256": sha(path)}


def clean_output(path):
    """Discard collapsed zero-area solver faces and orphan vertices, preserving raw output."""
    vertices, faces = [], []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            vertices.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("f "):
            faces.append([int(x.split("/")[0]) - 1 for x in line.split()[1:]])
    v = np.array(vertices)
    cleaned, removed = [], 0
    for face in faces:
        assert min(face) >= 0 and max(face) < len(v)
        points = v[face]
        area = np.linalg.norm(np.cross(points, np.roll(points, -1, axis=0)).sum(axis=0))
        if area <= 1e-20:
            removed += 1
            continue
        simple = [x for i, x in enumerate(face) if x != face[i - 1]]
        assert len(simple) >= 3 and len(simple) == len(set(simple)), face
        cleaned.append(simple)
    assert cleaned
    used = sorted({i for face in cleaned for i in face})
    remap = {old: new + 1 for new, old in enumerate(used)}
    temporary = path.with_suffix(".clean.obj")
    assert not temporary.exists()
    with temporary.open("w", encoding="utf-8", newline="\n") as f:
        f.write("# CULT-ULHU Instant Meshes retopology; original source preserved.\n")
        for i in used:
            f.write("v " + " ".join(format(x, ".9g") for x in v[i]) + "\n")
        for face in cleaned:
            f.write("f " + " ".join(str(remap[i]) for i in face) + "\n")
    inspect_obj(temporary)
    raw = SAVED / (path.stem + "_raw.obj")
    assert path.resolve().is_relative_to(EXPORTS.resolve()) and raw.resolve().is_relative_to(SAVED.resolve())
    assert not raw.exists()
    raw_digest = sha(path)
    path.rename(raw)
    temporary.rename(path)
    return {"collapsed_zero_area_faces_removed": removed, "orphan_vertices_removed": len(v) - len(used), "raw_output_sha256": raw_digest}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instant-meshes", required=True, type=Path)
    parser.add_argument("--model", choices=GOALS)
    args = parser.parse_args()
    assert args.instant_meshes.is_file()
    SAVED.mkdir(parents=True, exist_ok=True)
    destination = EXPORTS / "remeshed"
    destination.mkdir(exist_ok=True)
    report = {"models": [], "errors": [], "untouched": ["mi_go", "yig"],
              "already_lightweight": {"shoggoths": 20302, "nodens": 6260}}
    for name in ([args.model] if args.model else GOALS):
        source = EXPORTS / "characters" / f"{name}.ply"
        before = sha(source)
        target = destination / f"{name}_remeshed.obj"
        version = 2
        while target.exists():
            target = destination / f"{name}_remeshed_v{version}.obj"
            version += 1
        working = SAVED / f"{name}_welded.ply"
        assert not working.exists(), "Preserve an existing temporary mesh"
        try:
            record = welded_input(source, working)
            # Pure-quad extraction regularly subdivides its intermediate cells
            # into ~4x as many faces. Compensate in the solver goal.
            command = [str(args.instant_meshes), "--threads", "2", "--deterministic",
                       "--rosy", "4", "--posy", "4", "--faces", str(GOALS[name] // 4),
                       "--smooth", "2", "--boundaries", "--output", str(target), str(working)]
            started = time.monotonic()
            with (SAVED / f"{name}.log").open("w", encoding="utf-8") as f:
                result = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT,
                                        creationflags=subprocess.CREATE_NO_WINDOW, timeout=1200)
            assert result.returncode == 0 and target.is_file(), name
            record.update(clean_output(target))
            record.update(inspect_obj(target))
            assert record["faces"] == record["quads"], "Expected pure quads"
            assert record["triangles_after_triangulation"] < record["source_triangles"]
            assert sha(source) == before, "Original PLY changed"
            record.update(model=name, output=target.relative_to(ROOT).as_posix(),
                          input_sha256=before, solver_face_goal=GOALS[name] // 4,
                          seconds=round(time.monotonic() - started, 2))
            report["models"].append(record)
            print(json.dumps(record), flush=True)
        except Exception as error:
            report["errors"].append({"model": name, "error": repr(error)})
            print(name, repr(error), flush=True)
        finally:
            # Exact generated scratch file only; keep the source, result and log.
            if working.exists() and working.resolve().is_relative_to(SAVED.resolve()):
                working.unlink()
            (destination / "remesh_report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    assert not report["errors"], report["errors"]


if __name__ == "__main__":
    main()
