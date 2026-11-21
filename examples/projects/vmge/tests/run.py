#!/usr/bin/env python3
"""VMGE acceptance, from any working directory. All outputs stay in build/."""
import argparse
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
PROJECT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/vmge-tests"
SUFFIX = ".exe" if os.name == "nt" else ""


def run(command, marker=None, failure=None, env=None, cwd=ROOT):
    result = subprocess.run([str(c) for c in command], cwd=cwd, env=env,
                            capture_output=True, text=True, errors="replace", timeout=240)
    output = result.stdout + result.stderr
    if failure:
        if result.returncode == 0 or failure not in output:
            raise RuntimeError(f"Expected failure {failure!r}:\n{output}")
    elif result.returncode or (marker and marker not in output):
        raise RuntimeError(f"Command failed ({result.returncode}): {command}\n{output}")
    return output


def check_obj(path, expected_cubes):
    vertices, faces, groups = [], [], []
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "v":
            vertices.append(tuple(float(v) for v in fields[1:]))
        elif fields[0] == "f":
            faces.append(tuple(int(v) for v in fields[1:]))
        elif fields[0] == "g":
            groups.append(fields[1])
    assert len(vertices) == expected_cubes * 8, vertices
    assert len(faces) == expected_cubes * 6, faces
    assert len(groups) == expected_cubes, groups
    for face in faces:
        assert len(face) == 4 and all(1 <= v <= len(vertices) for v in face), face
        cube = (face[0] - 1) // 8
        assert all((v - 1) // 8 == cube for v in face), face
        points = vertices[cube * 8:cube * 8 + 8]
        center = tuple(sum(p[a] for p in points) / 8 for a in range(3))
        a, b, c = (vertices[v - 1] for v in face[:3])
        u, v = tuple(b[j] - a[j] for j in range(3)), tuple(c[j] - a[j] for j in range(3))
        normal = (u[1]*v[2] - u[2]*v[1], u[2]*v[0] - u[0]*v[2], u[0]*v[1] - u[1]*v[0])
        assert sum(normal[j] * (a[j] - center[j]) for j in range(3)) > 0, face
    return vertices


def complete(output, mode, frames):
    assert re.search(rf"VMGE_{mode}_COMPLETE frames=\s*{frames}\s+live_handles=\s*0", output), output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--graphics", action="store_true", help="hidden-window rendering, persistence, editor/viewer and resize checks")
    parser.add_argument("--assets", action="store_true", help="also load original ignored examples/assets textures and ambient audio")
    parser.add_argument("--map", type=Path, help="also smoke-test a local legacy map without modifying it (requires --graphics)")
    parser.add_argument("--poison", action="store_true", help="poison rewound arenas to detect retained transient pointers")
    args = parser.parse_args()
    if (args.assets or args.map) and not args.graphics:
        parser.error("--assets and --map require --graphics")
    BUILD.mkdir(parents=True, exist_ok=True)
    (BUILD / "partial_assets/Brick").mkdir(parents=True, exist_ok=True)
    compiler = Path(os.environ.get("VYNEC", str(ROOT / ("vynec" + SUFFIX)))).resolve()
    env = {k: v for k, v in os.environ.items() if not k.startswith("VYNE_VMGE_")}
    env.update(VYNE_VMGE_ROOT=str(PROJECT), VYNE_VMGE_ASSETS="procedural", VYNE_VMGE_AUDIO="0",
               VYNE_VMGE_WIDTH="640", VYNE_VMGE_HEIGHT="360", VYNE_VMGE_WINDOWED="1",
               VYNE_VMGE_HIDDEN="1", VYNE_VMGE_CAPACITY="64", VYNE_VMGE_AUTOTEST="1")
    if args.poison:
        env["VYNE_POISON_ARENA"] = "1"
    env.update(VYNE_VMGE_WORLD_TEST_SCENE=str(BUILD / "world-roundtrip.vscene"),
               VYNE_VMGE_WORLD_OPTIONS_SCENE=str(BUILD / "world-options.vscene"),
               VYNE_VMGE_WORLD_EXPORT_PREFIX=str(BUILD / "world-exports"),
               VYNE_VMGE_WORLD_CONTROLS_PREFIX=str(BUILD / "world-controls"),
               VYNE_VMGE_STUDIO_PREFIX=str(BUILD / "studio"),
               VYNE_VMGE_WORLD_REVIEW_PREFIX=str(BUILD / "integrity"),
               VYNE_UI_TEST_ROOT=str(BUILD / "ui-imports"))
    cpu_tests = ("map", "collision", "scene", "world", "world_scene", "world_options", "world_exports", "world_integrity", "studio")
    gpu_tests = ("render", "scene_render", "ui", "ui_assets", "world_render", "world_settings_render")
    for name in cpu_tests + (gpu_tests if args.graphics else ()):
        binary = BUILD / name
        run([compiler, "--compile", "-o", binary, PROJECT / "tests" / (name + ".vy")], env=env)
        if name == "ui_assets":
            # Import collision tests need a fresh project: preserving prior imports
            # is correct production behavior, not a deterministic test fixture.
            with tempfile.TemporaryDirectory(prefix="ui-imports-", dir=BUILD) as directory:
                import_env = dict(env, VYNE_UI_TEST_ROOT=directory)
                run([str(binary) + SUFFIX], marker="VMGE_UI_ASSETS_OK", env=import_env)
        else:
            run([str(binary) + SUFFIX], marker="VMGE_" + name.upper() + "_OK", env=env)
        print("PASS:", name)
    pad_env = dict(env, VYNE_VMGE_SPAWN_PAD="1")
    run([str(BUILD / "studio") + SUFFIX], marker="VMGE_STUDIO_OK", env=pad_env)
    print("PASS: authored-camera and generated-pad Play policies")
    vertices = check_obj(BUILD / "roundtrip.obj", 2)
    assert vertices[0] == (8.0, 1.0, -8.0), vertices[0]
    assert vertices[8] == (1.0, 0.0, -5.0), vertices[8]
    print("PASS: OBJ bounds, indexing and outward winding")
    binary = BUILD / "vmge"
    run([compiler, "--compile", "-o", binary, PROJECT / "src/main.vy"], env=env)
    run([compiler, "build", "-o", BUILD / "project"], env=env, cwd=PROJECT)
    for name in ("map_builder", "map_load_test", "camera_test"):
        run([compiler, "--compile", "-o", BUILD / name, ROOT / f"examples/graphics/{name}.vy"], env=env)
    print("PASS: VMGE entry point, project build, historical launchers and existing camera compile")
    env["VYNE_VMGE_MODE"] = "invalid"
    run([str(binary) + SUFFIX], failure="VYNE_VMGE_MODE must be scene, editor, or play", env=env)
    env["VYNE_VMGE_MODE"] = "play"
    env["VYNE_VMGE_MAP"] = str(BUILD / "missing.dat")
    run([str(binary) + SUFFIX], failure="Map file not found", env=env)
    print("PASS: invalid mode and missing explicit map diagnostics")
    if args.graphics:
        map_path = BUILD / "edited.dat"
        map_path.write_text("-2,1,4,2,0;\n")
        env.update(VYNE_VMGE_MODE="editor", VYNE_VMGE_MAP=str(map_path),
                   VYNE_VMGE_EXPORT=str(BUILD / "edited.obj"), VYNE_VMGE_FRAMES="8")
        output = run([str(binary) + SUFFIX], marker="VMGE_EDITOR_COMPLETE", env=env)
        complete(output, "EDITOR", 8)
        assert re.search(r"cubes=\s*2\s+edits=\s*3", output), output
        records = [tuple(float(v) for v in row.split(",")) for row in map_path.read_text().split(";") if row.strip()]
        assert records == [(-2.0, 1.0, 4.0, 2.0, 0.0), (100.0, 1.0, 100.0, 2.0, 0.0)], records
        check_obj(BUILD / "edited.obj", 2)
        env.update(VYNE_VMGE_MODE="play", VYNE_VMGE_FRAMES="12")
        output = run([str(binary) + SUFFIX], marker="VMGE_VIEWER_COMPLETE", env=env)
        complete(output, "VIEWER", 12)
        assert re.search(r"cubes=\s*2\s+uniforms=\s*3", output), output
        print("PASS: editor cross-frame placement/duplicate/removal/save/export and viewer reload, zero handles")
        env["VYNE_VMGE_MAP"] = str(PROJECT / "maps/demo.dat")
        output = run([str(binary) + SUFFIX], marker="VMGE_VIEWER_COMPLETE", env=env, cwd=BUILD)
        complete(output, "VIEWER", 12)
        print("PASS: portable eight-slot scene from another working directory")
        output = run([compiler, "run", "-o", BUILD / "project"], marker="VMGE_VIEWER_COMPLETE", env=env, cwd=PROJECT)
        complete(output, "VIEWER", 12)
        print("PASS: project run from VMGE directory")
        failure_env = dict(env, VYNE_VMGE_ASSETS=str(BUILD / "partial_assets"))
        output = run([str(binary) + SUFFIX], failure="Could not load texture", env=failure_env)
        assert re.search(r"VMGE_ERROR live_handles=\s*0", output), output
        print("PASS: partial asset load failure closes already-loaded resources")
        studio_env = dict(env, VYNE_VMGE_MODE="scene", VYNE_VMGE_SCENE=str(BUILD / "editor-scene.vscene"),
                          VYNE_VMGE_FRAMES="16", VYNE_VMGE_SCREENSHOT=str(BUILD / "scene-editor.png"))
        output = run([str(binary) + SUFFIX], marker="VMGE_SCENE_EDITOR_COMPLETE", env=studio_env)
        complete(output, "SCENE_EDITOR", 16)
        assert re.search(r"nodes=\s*5\s+play_cycles=\s*1\s+errors=\s*0", output), output
        assert re.search(r"commit_drift=\s*0", output), output
        assert (BUILD / "editor-scene.vscene").read_text().startswith("VMGE_SCENE|1\n")
        print("PASS: 3D workspace edits, hierarchy, undo/redo, save, isolated Play/Stop, resize and zero handles")
        extended_env = dict(studio_env, VYNE_VMGE_STUDIO_AUTOTEST="1",
                            VYNE_VMGE_SCENE=str(BUILD / "studio-flow.vscene"),
                            VYNE_VMGE_STUDIO_PREFIX=str(BUILD / "studio-flow"),
                            VYNE_VMGE_SCREENSHOT=str(BUILD / "studio-flow.png"))
        output = run([str(binary) + SUFFIX], marker="VMGE_STUDIO_AUTOTEST_OK", env=extended_env)
        complete(output, "SCENE_EDITOR", 16)
        assert re.search(r"nodes=\s*5\s+play_cycles=\s*1\s+errors=\s*0", output), output
        assert re.search(r"commit_drift=\s*0", output), output
        saved = (BUILD / "studio-flow.vscene").read_text()
        assert saved.startswith("VMGE_SCENE|2\n") and re.search(r"^G\|1(?:\.0)?\|", saved, re.MULTILINE), saved
        check_obj(BUILD / "studio-flow.obj", 2)
        print("PASS: terrain preview/strokes, modal capture, dirty document recovery, G metadata and resource-only exports")
        if args.assets:
            env.update(VYNE_VMGE_ASSETS=str(ROOT / "examples/assets"), VYNE_VMGE_AUDIO="1")
            output = run([str(binary) + SUFFIX], marker="VMGE_VIEWER_COMPLETE", env=env)
            complete(output, "VIEWER", 12)
            assert re.search(r"VMGE_AUDIO active=\s*true", output), output
            camera_env = dict(env, VYNE_CAMERA_FRAMES="30", VYNE_CAMERA_WIDTH="640", VYNE_CAMERA_HEIGHT="360", VYNE_CAMERA_AUDIO="0")
            output = run([str(BUILD / "camera_test") + SUFFIX], marker="BODYCAM_COMPLETE", env=camera_env)
            assert re.search(r"frames=\s*30\s+live_handles=\s*0", output), output
            print("PASS: original textures/dirt/music and existing bodycam regression")
        if args.map:
            local_env = dict(env, VYNE_VMGE_MAP=str(args.map.resolve()), VYNE_VMGE_CAPACITY="65536", VYNE_VMGE_AUTOTEST="0", VYNE_VMGE_FRAMES="8")
            for name, mode in (("map_builder", "EDITOR"), ("map_load_test", "VIEWER")):
                output = run([str(BUILD / name) + SUFFIX], marker=f"VMGE_{mode}_COMPLETE", env=local_env)
                complete(output, mode, 8)
            print("PASS: local legacy map renders in both historical launchers, zero handles")
    print("All requested VMGE checks passed.")


if __name__ == "__main__":
    main()
