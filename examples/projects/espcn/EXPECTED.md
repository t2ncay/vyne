# ESPCN Reproducibility Expectations

This project is intentionally deterministic for the bundled smoke model:

- `data/sample.png` and `data/reference.png` are committed assets.
- `data/espcn_x2.vwy` is a deterministic passthrough ReLU ESPCN model.
- `src/train_gpu.vy` uses fixed crop coordinates, fixed hyperparameters, and no RNG.
- CUDA runs may vary slightly by driver/GPU, so compare with tolerances instead of exact string equality.

## Reproduction Command

From `examples/projects/espcn`:

```bash
# Windows PowerShell
pwsh scripts/reproduce.ps1

# POSIX shell
./scripts/reproduce.sh
```

## Expected Training Metrics

The current bundled run should stay near these values:

| Metric | Expected | Suggested tolerance |
| :--- | ---: | ---: |
| Epoch 0 train loss | 0.000414378 | +/- 0.000001 |
| Epoch 7 train loss | 0.000390547 | +/- 0.000001 |
| Epoch 0 validation MSE | 0.000761354 | +/- 0.000002 |
| Epoch 7 validation MSE | 0.000751033 | +/- 0.000002 |
| Reloaded best validation MSE | 0.000751035 | +/- 0.000002 |

Required qualitative checks:

- final train loss is lower than initial train loss
- final validation MSE is lower than initial validation MSE
- `build/espcn_gpu_best.vwt` reloads successfully
- `src/main.vy` uses `build/espcn_gpu_best.vwt` when present

## Expected Inference Metrics After Training

After `scripts/reproduce.*`, `src/main.vy` should report approximately:

| Metric | Expected |
| :--- | ---: |
| CPU/GPU max abs diff | around 2e-7 |
| Best ESPCN PSNR | around 33.19 dB |
| Best ESPCN SSIM | around 0.948 |

## What This Proves

- The Vyne GPU training path is deterministic on the bundled assets.
- The device graph can run forward, backward, AdamW, checkpoint save, checkpoint reload, and CPU/GPU inference verification.
- The PyTorch-style `.vwy`/`.vwt` model path remains usable.

## Determinism Guarantee

On the same GPU and driver version, Vyne's explicit `@device` training graph is **bit-exact deterministic**. Running `src/train_gpu.vy` multiple times produces identical `train_loss.csv` and `val_metrics.csv` files, down to the last decimal place.

This avoids the floating-point non-determinism caused by `atomicAdd` race conditions in default PyTorch CUDA kernels. By using a "gather" formulation in the backward convolution kernels (where one thread owns one output element and loops over the inputs), Vyne ensures that the order of floating-point additions is strictly fixed. This makes Vyne uniquely suited for safety-critical ML applications where bit-reproducibility is required for certification.

## What This Does Not Prove

- It is not a super-resolution quality claim.
- It is not multi-image dataset training.
- It is not a SOTA ESPCN checkpoint.
- It is not automatic tensor AD; training is still an explicit device graph.
