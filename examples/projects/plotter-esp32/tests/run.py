"""Plotting acceptance suite. All generated artifacts stay in this project's build/.

python examples/projects/plotter-esp32/tests/run.py
python examples/projects/plotter-esp32/tests/run.py --esp32 --port COM5
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[2]
BUILD = PROJECT / "build"
COMPILER = Path(os.environ.get("VYNEC", ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))).resolve()


def command(args, cwd=PROJECT, timeout=300):
    result = subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise AssertionError(f"{args}\n{result.stdout}\n{result.stderr}")
    print(result.stdout.strip())
    return result.stdout


def device_capture(port, marker, seconds=60):
    import serial
    output=bytearray()
    with serial.Serial(port,115200,timeout=.2) as uart:
        uart.dtr=False;uart.rts=True;time.sleep(.1);uart.reset_input_buffer();uart.rts=False
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            output.extend(uart.read(4096))
            if marker.encode() in output:break
    return output.decode(errors="replace")


def png_info(path, expected_size=(400,300)):
    import struct
    import zlib
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    width, height, depth, kind = struct.unpack(">IIBB", data[16:26])
    assert (width, height, depth, kind) == (*expected_size, 8, 2)
    offset = 8
    compressed = bytearray()
    while offset < len(data):
        size = int.from_bytes(data[offset:offset+4], "big")
        tag = data[offset+4:offset+8]
        payload = data[offset+8:offset+8+size]
        assert zlib.crc32(tag + payload) & 0xFFFFFFFF == int.from_bytes(data[offset+8+size:offset+12+size], "big")
        if tag == b"IDAT":
            compressed.extend(payload)
        offset += size + 12
    raw = zlib.decompress(compressed)
    stride = width * 3
    rows = []
    previous = bytearray(stride)
    for y in range(height):
        mode = raw[y*(stride+1)]
        row = bytearray(raw[y*(stride+1)+1:(y+1)*(stride+1)])
        for x in range(stride):
            left = row[x-3] if x >= 3 else 0
            up = previous[x]
            upper_left = previous[x-3] if x >= 3 else 0
            if mode == 1: predictor = left
            elif mode == 2: predictor = up
            elif mode == 3: predictor = (left+up)//2
            elif mode == 4:
                p = left + up - upper_left
                distances = [abs(p-left), abs(p-up), abs(p-upper_left)]
                predictor = [left,up,upper_left][distances.index(min(distances))]
            else:
                assert mode == 0
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        rows.append(bytes(row))
        previous = row
    colors = {tuple(row[i:i+3]) for row in rows for i in range(0, stride, 3)}
    assert len(colors) >= 4, (path, colors)
    return {"width":width, "height":height, "colors":len(colors), "bytes":len(data)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--esp32", action="store_true")
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--sanitize", action="store_true", help="ASan/UBSan for the native C tests (supported toolchain required)")
    parser.add_argument("--core-only", action="store_true", help="Run C/C++ tests without a Vyne compiler")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    (BUILD / "plots").mkdir(exist_ok=True)
    exe = ".exe" if os.name == "nt" else ""
    sanitizers = ["-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"] if args.sanitize else []
    for name in ("core", "adapters", "bridge", "managed"):
        command(["gcc","-std=gnu11","-O2","-Wall","-Wextra",*sanitizers,f"tests/{name}.c","-o",BUILD/(name+"_tests"+exe),"-lm"])
        command([BUILD/(name+"_tests"+exe)])
    # C++ compatibility is required by the Arduino runtime.
    command(["g++","-std=c++17","-O2","-x","c++","tests/core.c","-o",BUILD/("core_cpp_tests"+exe),"-lm"])
    command([BUILD/("core_cpp_tests"+exe)])
    if args.core_only:
        print("PASS: portable core, band adapter pixel parity, bridge and C++ compatibility")
        return
    for name in ("api", "gallery", "aliases", "memory", "legacy", "friendly"):
        text = command([COMPILER,"--run","-o",BUILD/name,PROJECT/f"tests/{name}.vy"])
        assert f"VPLOT_{name.upper()}_PASS" in text
    results = {name:png_info(BUILD/f"plots/{name}.png") for name in ("line","scatter","bars","histogram","heatmap")}
    (BUILD / "host_results.json").write_text(json.dumps(results,indent=2)+"\n")
    for name in ("chart","line","scatter","bars","histogram","heatmap"):
        png_info(BUILD/f"quick_plots/{name}.png",(640,360))
    invalid = BUILD / "invalid"
    invalid.mkdir(exist_ok=True)
    for name, source, flags, diagnostic in [
        ("arity", "use native vplot; vplot.rgb(1,2);", [], "expects 3 arguments"),
        ("property_arity", "use native vplot; vplot.DARK(1);", [], "expects 0 arguments"),
        ("host_display", "use native vplot; vplot.display_surface(4096);", [], "requires the ESP32"),
        ("esp32_image", "use native vplot; vplot.image_surface(320,240);", ["--esp32","esp32"], "ESP32"),
    ]:
        path = invalid / (name+".vy")
        path.write_text(source)
        result = subprocess.run([str(COMPILER),*(flags or ["--compile"]),"-o",str(invalid/name),str(path)],cwd=PROJECT,capture_output=True,text=True)
        assert result.returncode and diagnostic in result.stdout+result.stderr, (name,result.stdout,result.stderr)
    print("PASS: native arity checks and incompatible backend compile diagnostics")
    with tempfile.TemporaryDirectory(prefix="init_vplot_",dir=BUILD) as path:
        command([COMPILER,"init",path,"--name","vplot_init","--kind","blank","--libraries","vplot","--no-git"])
        assert 'vplot = "0.1"' in (Path(path)/"project.toml").read_text()
    with tempfile.TemporaryDirectory(prefix="init_chart_",dir=BUILD) as path:
        command([COMPILER,"init",path,"--name","chart_init","--kind","embedded","--preset","hw950","--libraries","vplot","--port",args.port,"--no-git"])
        source=(Path(path)/"src/main.vy").read_text()
        assert "vplot.show_line" in source and "display_surface" not in source and "display_init" not in source and "use native esp32" not in source
        assert len(source.splitlines())<=6,source
        if args.esp32:
            command([COMPILER,"run","--port",args.port],cwd=Path(path),timeout=600)
            fresh_ino=(Path(path)/"build/firmware/main_sketch/main_sketch.ino").read_text()
            assert "vplot_show_line(" in fresh_ino and "esp32_display_init(" not in fresh_ino
            (BUILD/"fresh_starter.vy").write_text(source)
            print("PASS: fresh-project short chart starter built and uploaded without display/surface setup")
    print("PASS: C/C++ core, native API, ownership, zero-allocation redraws, five decoded PNG chart types")
    if args.esp32:
        command([COMPILER,"run","tests/device.vy","--port",args.port], timeout=600)
        import serial
        device_text=device_capture(args.port,"VPLOT_DEVICE_PASS")
        (BUILD / "esp32_device.log").write_text(device_text)
        assert "VPLOT_DEVICE_PASS" in device_text and "VPLOT_DEVICE_FAIL" not in device_text, device_text
        command([COMPILER,"run","tests/friendly_device.vy","--port",args.port],timeout=600)
        friendly_text=device_capture(args.port,"VPLOT_FRIENDLY_DEVICE_PASS",90)
        (BUILD/"esp32_friendly.log").write_text(friendly_text)
        assert "VPLOT_FRIENDLY_DEVICE_PASS" in friendly_text and "VPLOT_FRIENDLY_FAIL" not in friendly_text,friendly_text
        command([COMPILER,"run","tests/friendly_live.vy","--port",args.port], timeout=600)
        output = bytearray()
        with serial.Serial(args.port,115200,timeout=.1) as uart:
            uart.dtr=False
            uart.rts=True
            time.sleep(.1)
            uart.reset_input_buffer()
            uart.rts=False
            deadline=time.monotonic()+args.seconds
            while time.monotonic()<deadline:
                output.extend(uart.read(4096))
        text=output.decode(errors="replace")
        (BUILD / "esp32_serial.log").write_text(text)
        assert "VPLOT_READY" in text, text
        assert "VPLOT_SIZE 320x240" in text, text
        samples = [tuple(map(int,m)) for m in re.findall(r"VPLOT_STATUS frames=(\d+) samples=(\d+) native_bytes=(\d+) allocations=(\d+) heap_free=(\d+) arena=(\d+)",text)]
        assert len(samples)>=3, text
        assert samples[-1][0]>samples[0][0] and samples[-1][1]<=512
        assert len({s[2] for s in samples})==1 and len({s[3] for s in samples})==1 and len({s[5] for s in samples})==1, samples
        assert max(s[4] for s in samples)-min(s[4] for s in samples)<=4096, samples
        assert not re.search(r"Guru Meditation|panic|invalid plotting|allocation failed",text,re.I),text
        (BUILD / "esp32_results.json").write_text(json.dumps({"frames":samples[-1][0],"samples":samples[-1][1],"native_bytes":samples[-1][2],"allocations":samples[-1][3],"heap_free_min":min(s[4] for s in samples),"heap_free_max":max(s[4] for s in samples),"arena":samples[-1][5],"result":"PASS"},indent=2)+"\n")
        print("PASS: ESP32 live frames, ring capacity, native allocation count and arena/heap ownership stability")
        command([COMPILER,"run","--port",args.port],timeout=600) # leave the short user-facing demo running


if __name__ == "__main__":
    main()
