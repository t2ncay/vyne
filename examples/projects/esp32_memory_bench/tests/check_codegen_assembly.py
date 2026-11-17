"""Audit actual ESP32 inference assembly, retaining disassembly and hash evidence."""
import importlib.util
import json
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bench", ROOT / "scripts/bench.py")
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


def main():
    for profile in ("default", "sized"):
        directory = bench.BUILD / profile / "vyne"
        properties = dict(line.split("=", 1) for line in
                          (directory / "properties.txt").read_text().splitlines() if "=" in line)
        tool = Path(properties["compiler.path"]) / properties["compiler.size.cmd"].replace("-size", "-objdump")
        if os.name == "nt" and not tool.exists():
            tool = tool.with_suffix(".exe")
        elf = directory / "firmware/sketch.ino.elf"
        record = json.loads((directory / "build.json").read_text())
        if bench.sha(elf) != record["elf_sha256"]:
            raise AssertionError("ELF changed after build")
        assembly = bench.command([tool, "-d", "-C", "--disassemble=vyne_setup()", elf],
                                 log=directory / "vyne-setup.asm")
        for opcode in ("mul.s", "add.s"):
            if not re.search(r"\b" + re.escape(opcode) + r"\b", assembly):
                raise AssertionError(f"missing native Float32 {opcode}")
        for helper in ("__extendsfdf2", "__truncdfsf2"):
            if helper in assembly:
                raise AssertionError("double conversion remains in inference setup: " + helper)
        if "fn_bench_weight1_native" in assembly or "fn_bench_weight2_native" in assembly:
            raise AssertionError("weight accessors were not inlined")
        source = (directory / "emitted.vy.c").read_text()
        body = source[source.index("void vyne_setup()") :]
        hot = body.split("int64_t v_iteration", 1)[1].split("arduino_timer_read()", 1)[0]
        if any(name in hot for name in ("vyne_binop(", "vyne_float32(", "vyne_to_float32(")):
            raise AssertionError("boxing remains in emitted MAC loop")
        audit = {"profile": profile, "compiler_sha256": record["compiler_sha256"],
                 "elf_sha256": record["elf_sha256"], "assembly_sha256": bench.sha(directory / "vyne-setup.asm"),
                 "native_float_mul_add": True, "float32_double_helpers_in_setup": False,
                 "weight_accessors_inlined": True, "boxed_mac_loop": False,
                 "setup_generic_dispatch_references": assembly.count("<vyne_binop(")}
        audit["compiler_source_sha256"] = {name: bench.sha(bench.REPO / name) for name in
            ("vyne/compiler/ast/ast.h", "vyne/compiler/codegen/builtins.cpp")}
        # Preserve the measured working-tree patch when the change is later
        # committed; rerunning an audit must not replace it with an empty diff.
        if not (directory / "compiler.patch").exists():
            bench.command(["git", "-C", bench.REPO, "diff", "--", "vyne/compiler/ast/ast.h",
                           "vyne/compiler/codegen/builtins.cpp"], log=directory / "compiler.patch")
        audit["compiler_patch_sha256"] = bench.sha(directory / "compiler.patch")
        (directory / "codegen-audit.json").write_text(json.dumps(audit, indent=2) + "\n")
        print("CODEGEN_ASSEMBLY_PASS " + json.dumps(audit))


if __name__ == "__main__":
    main()
