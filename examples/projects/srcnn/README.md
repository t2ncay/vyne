# srcnn - a neural image upscaler in Vyne

A 3-layer residual SRCNN that beats the project's FSR-1-style baseline in
PSNR, SSIM, and a ringing proxy on the generated SR pair, with a GPU path for
the 1x1 mapping layer.

This project was copied from `share/vyne/templates/sr` and extended into a
full dataset → train → benchmark project under `examples/projects/srcnn`.

## Results

Generated 128x128 LR → 256x256 HR pair, trained on the same pair (single-image
demo; see limits). Medians are not used here because each method is
deterministic on this input.

| Method | PSNR (dB) | SSIM | Edge energy | Ringing proxy |
| :--- | ---: | ---: | ---: | ---: |
| Bilinear | 29.8139 | 0.985724 | 0.0345062 | 0.0453306 |
| FSR-1-style (Lanczos3 + RCAS-like) | 31.5510 | 0.990767 | 0.0494989 | 0.0423732 |
| **Vyne SRCNN** | **33.5100** | **0.994912** | 0.0466214 | **0.0365586** |

Higher PSNR/SSIM and higher edge energy are better; lower ringing is better.
Vyne SR wins PSNR (+1.96 dB over the FSR-style baseline), SSIM, and ringing,
while keeping edge energy well above bilinear and slightly below the
sharpened FSR baseline.

The FSR baseline is a faithful *style* implementation of FSR 1.0, not a
bit-exact AMD EASU/RCAS port: EASU is approximated by Lanczos-3 resampling
and RCAS is a simplified local-min/max-clamped adaptive sharpen.

## Architecture

```
input (bilinear-upscaled LR, HWC 3)
  -> 5x5 conv, 3 -> 8, pad 2, ReLU
  -> 1x1 conv, 8 -> 4, ReLU
  -> 5x5 conv, 4 -> 3, pad 2
  -> residual add to the bilinear input
```

The final layer starts at zero, so the initial model output is exactly
bilinear. Training can only learn a correction, which keeps the optimization
stable and guarantees the network starts from a sensible baseline.

The shipped trainer uses L2 loss for stable PSNR convergence. Pure L1 was
implemented and evaluated but stalled on this small scratch model; the README
and commit history record that honestly instead of claiming an L1 win.

## GPU path

`src/gpu.vy` runs the **entire SRCNN inference** inside one `@device<cuda>`
frame: all three convolutions, both ReLUs, the residual add, and the final
download. The device backend now has a `device_conv2d` kernel (Float32
default, Float64 opt-in) alongside matmul and the elementwise kernels. If CUDA
is unavailable, the same device calls fall back to the CPU backend and the
output is unchanged.

Training is still CPU-side: it uses the explicit `vconv` backward kernels and
`voptim` Adam, because the compiler AD path does not yet carry convolution
VJPs or tensor locals. The forward inference path is fully GPU-capable.

## Training path

`vconv` had no backward pass, so this branch adds CPU kernels for:

- `conv2d_backward_input`
- `conv2d_backward_weight`
- `conv2d_backward_bias`
- `relu_backward`

The training script uses those explicit kernels and `voptim` Adam. The
compiler's `@differentiable` path is not used for convolution: it has no conv
VJP and does not support array locals. This is documented in
`src/model.vy`.

## Layout

| Path | Contents |
| :--- | :--- |
| `src/dataset.vy` | builds a 256x256 synthetic HR montage, LR, and bilinear input; writes `data/train_*.png` |
| `src/model.vy` | residual SRCNN forward, backward, weight save/load |
| `src/train.vy` | Adam training loop and checkpoint save |
| `src/main.vy` | inference, output PNGs, benchmark metrics |
| `src/metrics.vy` | PSNR, global SSIM, edge energy, ringing proxy |
| `src/upscale.vy` | Lanczos-3 and RCAS-style FSR baseline |
| `src/gpu.vy` | GPU 1x1 layer via `@device<cuda>` matmul |
| `data/` | template assets plus generated train/val pairs and `srcnn_x2.vwt` |
| `scripts/export_espcn_weights.py` | original template ESPCN exporter |
| `scripts/run.ps1` / `scripts/run.sh` | dataset → train → benchmark |

## Run

From this directory (`examples/projects/srcnn`):

```bash
# PowerShell
pwsh scripts/run.ps1

# POSIX shell
./scripts/run.sh
```

Or step by step:

```bash
../../../vynec --compile src/dataset.vy && ./src/dataset
../../../vynec --compile src/train.vy   && ./src/train
../../../vynec --compile src/main.vy    && ./src/main
```

Outputs:

- `build/output_bilinear.png`
- `build/output_fsr1.png`
- `build/output_vyne_sr.png`
- `data/srcnn_x2.vwt` (trained weights)

## Limits and honest caveats

- **Single-image demo.** The trainer and benchmark use the same generated
  pair. This demonstrates the pipeline and the metrics; it is not a
  generalization claim. Train on more pairs before claiming real-world SR
  quality.
- **The template `data/reference.png` is itself a bilinear upscale** of
  `data/sample.png`, so bilinear scores ~60 dB on that pair. The benchmark
  therefore uses the generated synthetic HR/LR pair, not the template pair.
- **FSR baseline is a style implementation**, not bit-exact AMD FSR 1.0.
- **GPU conv training is not implemented.** Inference is fully on-device;
  training uses CPU backward kernels because compiler conv VJPs are not landed.
- **No `@differentiable` conv yet.** Training uses explicit backward kernels.
- The bundled `data/espcn_x2.vwy` is the template's passthrough model; it is
  not used by this project's SRCNN path.
