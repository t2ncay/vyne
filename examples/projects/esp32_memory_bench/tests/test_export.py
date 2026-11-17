"""VWT corruption/roundtrip tests; run in the pinned export environment."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("export", ROOT / "scripts/train_and_export.py")
export = importlib.util.module_from_spec(spec)
spec.loader.exec_module(export)


class ExportContracts(unittest.TestCase):
    def test_float32_roundtrip(self):
        import numpy as np
        tensors = {name: np.linspace(-1.2345678, 1.2345678, shape[0] * shape[1], dtype=np.float32).reshape(shape)
                   for name, shape in export.SHAPES.items()}
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / "model.vwt"
            export.write_vwt(file, tensors)
            actual = export.read_vwt(file)
            for name in tensors:
                np.testing.assert_array_equal(actual[name], tensors[name])

    def test_corrupt_shapes_names_and_values(self):
        import numpy as np
        tensors = {name: np.zeros(shape, dtype=np.float32) for name, shape in export.SHAPES.items()}
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / "model.vwt"
            export.write_vwt(file, tensors)
            original = file.read_text()
            replacements = (("VYNE_MLP_F32_V1", "OTHER"), ("fc1.weight|32|64", "fc1.weight|32|63"),
                            ("fc2.bias", "fc1.bias"), ("fc2.bias|10|1|0", "fc2.bias|10|1|nan"))
            for pair in replacements:
                with self.subTest(pair=pair):
                    file.write_text(original.replace(*pair))
                    with self.assertRaises(ValueError):
                        export.read_vwt(file)


if __name__ == "__main__":
    unittest.main()
