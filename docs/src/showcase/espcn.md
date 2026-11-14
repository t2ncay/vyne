---
id: espcn
title: ESPCN Inference & Training
group: Showcase
icon: fa-network-wired
order: 20
description: Offline ESPCN inference, explicit device-resident GPU training, checkpoints, metrics, and reproducibility.
---

# ESPCN Inference & Training

`examples/projects/espcn` is a standalone super-resolution project:
low-resolution input → three convolutions → pixel shuffle → high-resolution output.
It demonstrates CPU exact inference and a Float32 full-device ReLU path, with
bilinear and FSR-style non-neural baselines and PSNR/SSIM comparisons.

The committed model is a deterministic **nearest-neighbour 2× passthrough**.
It exercises model loading and the real graph offline; it is not a trained
super-resolution quality result. The sample and reference images are committed.

## Run inference

From the repository root, with `vynec` on PATH:

```shell
vynec run examples/projects/espcn
```

Or from the project directory:

```shell
# POSIX
./scripts/run.sh
# PowerShell
pwsh scripts/run.ps1
```

The runner writes comparison images under `build/`: `output_bilinear.png`,
`output_fsr1_like.png`, `output_espcn_cpu.png`, and `output_espcn_gpu.png` when
the activation is supported. It reports CPU/GPU timings, max absolute difference,
PSNR/SSIM, edge energy, and a ringing proxy. CUDA requests fall back to the CPU
device backend when no compatible driver/device is available; check the reported
backend before interpreting GPU timing.

## Train and reproduce

From `examples/projects/espcn`:

```shell
# Clean build/, smoke-test exporter, train, reload best checkpoint, infer, validate
pwsh scripts/reproduce.ps1
# POSIX equivalent
./scripts/reproduce.sh
```

For training alone:

```shell
vynec --run src/train_gpu.vy
```

The graph allocates buffers in a single `@device<cuda>` region and uploads the
three deterministic training crops plus one held-out crop once. It reuses
forward activations, gradient buffers, AdamW moments, and device loss storage.
`vtensor.copy_slice` stages crops, `vnn` runs forward/backward blocks, and device
reduction downloads one loss scalar instead of a full image.

The project writes `train_loss.csv`, `val_metrics.csv`, latest and best weight
checkpoints under `build/`, then reloads the best checkpoint through `vconv` and
checks CPU inference. Inference chooses `build/espcn_gpu_best.vwt`, then
`build/espcn_gpu_trained.vwt`, then the bundled `data/espcn_x2.vwy`.

## Reproducibility expectations

The bundled graph uses fixed crops/hyperparameters and no RNG. Its documented
guarantee is **bit-exact repeatability on the same GPU and driver version**:
repeat runs produce identical loss/validation CSVs. Backward convolution uses
gather-style fixed-order sums instead of atomic accumulation. Across different
hardware or driver versions, use tolerances rather than exact output equality.

| Metric | Expected bundled run | Suggested tolerance |
| :--- | ---: | ---: |
| Epoch 0 training loss | 0.000414378 | ±0.000001 |
| Epoch 7 training loss | 0.000390547 | ±0.000001 |
| Epoch 0 validation MSE | 0.000761354 | ±0.000002 |
| Epoch 7 validation MSE | 0.000751033 | ±0.000002 |
| Reloaded best validation MSE | 0.000751035 | ±0.000002 |

Training and validation loss should decrease; the best checkpoint must reload.
Post-training inference is approximately 33.19 dB PSNR, 0.948 SSIM, and 2e-7
CPU/GPU max absolute difference for the bundled assets. These numbers describe
this smoke workload, not general model quality or performance.
The maintained [expectations](https://github.com/t2ncay/vyne/blob/master/examples/projects/espcn/EXPECTED.md)
are the reference for reproduction results.

## Use a real checkpoint

```shell
python scripts/export_espcn_weights.py --checkpoint path/to/espcn_x2.pth --scale 2 --activation relu --out data/espcn_x2.vwy
```

The exporter accepts a state dict, an `nn.Module`, or a checkpoint containing
`state_dict` / `model`; loading real PyTorch checkpoints requires PyTorch.
Bundle metadata carries dimensions, scale, kernels, and activation. ReLU has
full-device inference support; tanh/leaky-ReLU checkpoints use the CPU exact path.
Existing trained build checkpoints take precedence, so use a clean build directory
when evaluating a newly exported bundled model.

## Current scope

Training uses ReLU ESPCN, batch size 1, fixed crops, MSE, and AdamW.
It is an explicit graph over [vtensor](page:vtensor) and [vnn](page:vnn), not
automatic tensor AD or a multi-image dataset trainer. The checkpoint/reload and
forward/backward/update lifecycle is the demonstration.

## See also

- [Project source and guide](https://github.com/t2ncay/vyne/tree/master/examples/projects/espcn)
- [CUDA & Devices](page:device), [vtensor](page:vtensor), and [vnn](page:vnn)
- [vconv](page:vconv) and [SRCNN Showcase](page:srcnn)
