# Super-resolution demo assets

`examples/espcn_inference.vy` runs with no assets: it generates a 64x64
test pattern and uses a built-in passthrough 2x model. To use a real
low-resolution input and/or a trained ESPCN checkpoint, drop files in
`examples/assets/sr/` (that directory is gitignored, so local images
never leak into a commit):

| File | Meaning |
| --- | --- |
| `input.png` (or `.jpg`) | low-resolution input |
| `input_hr.png` | optional ground truth at the upscaled size; enables PSNR/SSIM output |

The model is read from `models/espcn_x2.vwy`. Produce it from a PyTorch
checkpoint with:

```bash
python scripts/export_espcn_weights.py \
    --checkpoint path/to/espcn_x2.pth \
    --scale 2 \
    --activation tanh \
    --out models/espcn_x2.vwy
```

The exporter accepts a plain `state_dict`, a full `nn.Module`, or a
checkpoint dict containing either. It normalises the common key
spellings (`conv1.weight`, `0.weight`, ...) and infers the scale from
the last convolution's channel count.

For a quick loader smoke test without PyTorch:

```bash
python scripts/export_espcn_weights.py \
    --random --scale 2 --out models/espcn_x2.vwy
```

That writes the same deterministic passthrough network the example
builds internally (conv1/conv2 are 1x1 identities, conv3 fans each
channel into its 2x2 sub-pixels). It is nearest-neighbour upscaling, not
a quality model; it exists to prove the weight-bundle path.

Then:

```bash
./vynec --compile examples/espcn_inference.vy
./examples/espcn_inference
# -> build/espcn_output.png
```

If `input_hr.png` is present and matches the output size, the program
prints PSNR and SSIM for both a bilinear baseline and the network.
