"""Seeded PyTorch training -> versioned VWT -> equivalent Float32 TFLite + headers."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path

os.environ.setdefault("TF_ENABLE_ONEDNN_OPTS", "0")
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")
ROOT = Path(__file__).resolve().parents[1]
SHAPES = {"fc1.weight": (32, 64), "fc1.bias": (32, 1),
          "fc2.weight": (10, 32), "fc2.bias": (10, 1)}


def write_vwt(path, tensors):
    lines = ["VYNE_MLP_F32_V1"]
    for name, shape in SHAPES.items():
        array = tensors[name].reshape(shape)
        lines.append(f"{name}|{shape[0]}|{shape[1]}|" +
                     ",".join(format(float(x), ".9g") for x in array.flat))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_vwt(path):
    import numpy as np
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "VYNE_MLP_F32_V1" or len(lines) != 5:
        raise ValueError("invalid VWT header/tensor count")
    tensors = {}
    for line in lines[1:]:
        name, rows, cols, values = line.split("|")
        shape = (int(rows), int(cols))
        if name in tensors or name not in SHAPES or shape != SHAPES[name]:
            raise ValueError("invalid VWT tensor name/shape")
        array = np.array([float(x) for x in values.split(",")], dtype=np.float32)
        if array.size != shape[0] * shape[1] or not np.isfinite(array).all():
            raise ValueError("invalid VWT values")
        tensors[name] = array.reshape(shape)
    if set(tensors) != set(SHAPES):
        raise ValueError("missing VWT tensor")
    return tensors


def c_array(name, array):
    values = []
    for value in array.flat:
        text = format(float(value), ".9g")
        if "." not in text and "e" not in text.lower():
            text += ".0"
        values.append(text + "f")
    rows = [", ".join(values[i:i + 8]) for i in range(0, len(values), 8)]
    return f"static const float {name}[{array.size}] = {{\n  " + ",\n  ".join(rows) + "\n};\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=20261117)
    parser.add_argument("--epochs", type=int, default=5)
    parser.add_argument("--output", type=Path, default=ROOT / "data")
    args = parser.parse_args()
    if args.epochs < 1:
        parser.error("epochs must be positive")
    import numpy as np
    import torch
    import tensorflow as tf
    from tensorflow.lite.python import schema_py_generated as schema

    torch.set_num_threads(1)
    torch.manual_seed(args.seed)
    torch.use_deterministic_algorithms(True)
    tf.keras.utils.set_random_seed(args.seed)
    tf.config.threading.set_inter_op_parallelism_threads(1)
    tf.config.threading.set_intra_op_parallelism_threads(1)

    class MLP(torch.nn.Module):
        def __init__(self):
            super().__init__()
            self.fc1 = torch.nn.Linear(64, 32)
            self.fc2 = torch.nn.Linear(32, 10)

        def forward(self, x):
            return self.fc2(torch.relu(self.fc1(x)))

    model = MLP()
    inputs = torch.randn(512, 64)
    teacher = torch.randn(64, 10)
    labels = (inputs @ teacher).argmax(dim=1)
    optimizer = torch.optim.Adam(model.parameters(), lr=0.003)
    losses = []
    for _ in range(args.epochs):
        loss = torch.nn.functional.cross_entropy(model(inputs), labels)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        losses.append(float(loss.detach()))
    model.eval()
    tensors = {name: value.detach().numpy().astype(np.float32)
               for name, value in model.state_dict().items()}
    args.output.mkdir(parents=True, exist_ok=True)
    write_vwt(args.output / "model.vwt", tensors)
    reread = read_vwt(args.output / "model.vwt")
    for name in SHAPES:
        if not np.array_equal(tensors[name].reshape(SHAPES[name]), reread[name]):
            raise AssertionError("VWT Float32 roundtrip mismatch")

    x = tf.keras.Input(shape=(64,), batch_size=1, dtype=tf.float32, name="input")
    fc1 = tf.keras.layers.Dense(32, activation="relu", name="fc1")
    fc2 = tf.keras.layers.Dense(10, name="fc2")
    equivalent = tf.keras.Model(x, fc2(fc1(x)))
    fc1.set_weights([reread["fc1.weight"].T, reread["fc1.bias"].ravel()])
    fc2.set_weights([reread["fc2.weight"].T, reread["fc2.bias"].ravel()])
    converter = tf.lite.TFLiteConverter.from_keras_model(equivalent)
    converter.optimizations = []
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS]
    flatbuffer = converter.convert()
    interpreter = tf.lite.Interpreter(model_content=flatbuffer,
                                     experimental_preserve_all_tensors=True)
    interpreter.allocate_tensors()
    test_inputs = torch.randn(8, 64).numpy().astype(np.float32)
    with torch.no_grad():
        expected = model(torch.from_numpy(test_inputs)).numpy()
    max_error = 0.0
    for input_vector, reference in zip(test_inputs, expected):
        keras_output = equivalent(input_vector[None, :], training=False).numpy()[0]
        interpreter.set_tensor(interpreter.get_input_details()[0]["index"], input_vector[None, :])
        interpreter.invoke()
        tflite_output = interpreter.get_tensor(interpreter.get_output_details()[0]["index"])[0]
        np.testing.assert_allclose(keras_output, reference, atol=2e-5, rtol=2e-4)
        np.testing.assert_allclose(tflite_output, reference, atol=2e-5, rtol=2e-4)
        max_error = max(max_error, float(np.max(np.abs(tflite_output - reference))))
    parsed = schema.Model.GetRootAsModel(flatbuffer, 0)
    operators = []
    for i in range(parsed.OperatorCodesLength()):
        code = parsed.OperatorCodes(i)
        builtin = max(code.BuiltinCode(), code.DeprecatedBuiltinCode())
        if builtin not in (schema.BuiltinOperator.FULLY_CONNECTED, schema.BuiltinOperator.RELU):
            raise ValueError(f"unexpected operator {builtin}")
        if code.Version() > 1:
            raise ValueError(f"unsupported operator version {code.Version()}")
        operators.append({"builtin": int(builtin), "version": int(code.Version())})
    for detail in interpreter.get_tensor_details():
        if detail["dtype"] != np.float32:
            raise ValueError("non-Float32 tensor in export")
    if interpreter.get_input_details()[0]["shape"].tolist() != [1, 64] or interpreter.get_output_details()[0]["shape"].tolist() != [1, 10]:
        raise ValueError("unexpected exported input/output shape")
    graph = parsed.Subgraphs(0)
    if parsed.SubgraphsLength() != 1 or graph.OperatorsLength() not in (2, 3):
        raise ValueError("unexpected model graph")
    (args.output / "model.tflite").write_bytes(flatbuffer)
    hex_rows = [", ".join(f"0x{x:02x}" for x in flatbuffer[i:i + 16])
                for i in range(0, len(flatbuffer), 16)]
    (args.output / "model.h").write_text(
        "#pragma once\n#include <stddef.h>\n#include <stdint.h>\n"
        "alignas(16) static const unsigned char g_model[] = {\n  " +
        ",\n  ".join(hex_rows) + "\n};\nstatic const size_t g_model_len = sizeof(g_model);\n",
        encoding="utf-8")
    header = "#pragma once\n"
    for name, array in reread.items():
        header += c_array("bench_" + name.replace(".", "_"), array)
    header += c_array("bench_inputs", test_inputs)
    header += c_array("bench_expected", expected)
    model_hash = hashlib.sha256(flatbuffer).hexdigest()
    header += f'#define BENCH_MODEL_SHA "{model_hash}"\n'
    (args.output / "weights.h").write_text(header, encoding="utf-8")
    manifest = {"schema": 1, "seed": args.seed, "epochs": args.epochs,
                "shape": [64, 32, 10], "activation": "hidden_relu_output_linear",
                "dtype": "float32", "test_vectors": 8, "losses": losses,
                "atol": 2e-5, "rtol": 2e-4, "max_export_error": max_error,
                "max_reference_abs": float(np.max(np.abs(expected))),
                "operators": operators, "versions": {"torch": torch.__version__,
                "tensorflow": tf.__version__, "numpy": np.__version__},
                "hashes": {name: hashlib.sha256((args.output / name).read_bytes()).hexdigest()
                           for name in ("model.vwt", "model.tflite", "model.h", "weights.h")}}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
