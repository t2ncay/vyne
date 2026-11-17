# Generated model artifacts

Run `scripts/train_and_export.py` in the pinned export environment. It creates:

- `model.vwt`: UTF-8 `VYNE_MLP_F32_V1`, followed by one line per layer tensor:
  `name|rows|columns|comma-separated float32 values` (output-major weights).
- `model.tflite` and `model.h`: identical trained MLP for TFLM, with aligned bytes.
- `weights.h`: const flash-resident Float32 weights, 8 test input vectors and
  reference outputs shared by both implementations.
- `manifest.json`: dimensions, seed, hashes, versions, numerical checks and operators.

The exporter reads back the VWT file before generating `weights.h`. Nine significant
decimal digits preserve Float32 round trips. The benchmark firmware never parses
text, opens a filesystem or trains on the device. Initialization and model export
are excluded from inference timing. Generated artifacts are ignored by Git; retain
them with raw reports when publishing a result. This synthetic classification task
validates the pipeline, not the model's usefulness or real-world accuracy.
