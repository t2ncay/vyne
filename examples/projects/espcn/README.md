# espcn - ESPCN super-resolution in Vyne

This project is a standalone ESPCN inference and benchmark example. It is built
from the useful shape of `examples/projects/srcnn`, but the source path is a real
ESPCN pipeline: low-resolution input -> three convolutions -> pixel shuffle ->
high-resolution output.

The project ships with a deterministic passthrough `data/espcn_x2.vwy` model so
it runs offline. That model performs nearest-neighbour 2x upscaling through the
same weight-loading, convolution, and pixel-shuffle path used by real ESPCN
checkpoints.

## What It Demonstrates

- CPU exact ESPCN inference with `vconv`.
- Full-device Float32 inference in one `@device<cuda>` region.
- Device-resident `conv2d -> ReLU -> conv2d -> ReLU -> conv2d -> pixel_shuffle`.
- Bilinear and FSR-style non-neural baselines.
- PSNR, SSIM, edge energy, ringing proxy, CPU/GPU timing, and max CPU/GPU diff.

CUDA is requested when running the GPU path. If the CUDA driver or device is not
available, Vyne's `@device<cuda>` backend falls back to the CPU device backend so
the same source remains runnable.

## Run

From `examples/projects/espcn`:

```bash
# POSIX shell
./scripts/run.sh

# or manually
../../../vynec --compile src/main.vy
./src/main
```

On Windows PowerShell:

```powershell
pwsh scripts/run.ps1
```

Outputs are written to `build/`:

- `output_bilinear.png`
- `output_fsr1_like.png`
- `output_espcn_cpu.png`
- `output_espcn_gpu.png` when the model activation is GPU-supported

## Use A Real ESPCN Checkpoint

Export a PyTorch checkpoint into the bundled model path:

```bash
python scripts/export_espcn_weights.py \
    --checkpoint path/to/espcn_x2.pth \
    --scale 2 \
    --activation relu \
    --out data/espcn_x2.vwy
```

The exporter accepts a plain `state_dict`, a full `nn.Module`, or a checkpoint
dict containing `state_dict` or `model`. The bundle metadata records scale,
channels, kernel sizes, and activation so `src/main.vy` adapts without source
edits.

For a smoke-test model:

```bash
python scripts/export_espcn_weights.py --random --scale 2 --out data/espcn_x2.vwy
```

## GPU Activation Support

The full-device path currently supports ReLU ESPCN checkpoints because the
device runtime has `device_relu`. Checkpoints exported with `tanh` or
`leaky_relu` still run through the CPU exact path; adding device tanh/leaky-ReLU
would make those fully device-resident too.

## Layout

| Path | Contents |
| :--- | :--- |
| `src/main.vy` | loads inputs/model, runs baselines and ESPCN, saves outputs |
| `src/model.vy` | ESPCN model metadata, weight loading, CPU exact forward |
| `src/gpu.vy` | one-frame `@device<cuda>` Float32 ESPCN inference |
| `src/metrics.vy` | PSNR/SSIM, edge/ringing, CPU/GPU max difference |
| `src/upscale.vy` | Lanczos-3 + RCAS-style baseline |
| `scripts/export_espcn_weights.py` | PyTorch -> `.vwy`/`.vwt` exporter |
| `data/sample.png` | low-resolution sample input |
| `data/reference.png` | high-resolution reference |
| `data/espcn_x2.vwy` | offline passthrough model |

## Next Step

## GPU Training V0

`src/train_gpu.vy` is the first explicit GPU-training proof for ESPCN. It uses a
single `@device<cuda>` region, saved device activations, device MSE gradient,
device pixel-shuffle backward, device conv backward kernels, ReLU backward, and
in-place device SGD updates.

```bash
../../../vynec --compile src/train_gpu.vy
./src/train_gpu
```

Scope and limits:

- ReLU ESPCN only.
- Batch size 1.
- MSE loss and SGD.
- Explicit training graph, not automatic tensor AD yet.
- The success criterion is that the loss decreases over the bundled sample pair.

The next training step is an AdamW device optimizer and a reusable external
`vnn`/`DeviceTensor` surface once the explicit proof stays stable.
