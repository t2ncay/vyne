# {{name}}

A Vyne image super-resolution project. It loads a PNG, runs a 3-layer
ESPCN-style network with `vconv`, and writes a 2x PNG with `vimage`.

## Build and run

From the project root:

```bash
vynec run src/main.vy
```

The output is written to `build/output.png`. The program also prints
PSNR against the bundled reference for both the network and a bilinear
baseline.

## The bundled model

`data/espcn_x2.vwy` is a deterministic passthrough model: 1x1 identity
convolutions plus a sub-pixel fan-out. It is nearest-neighbour 2x, not
a trained quality model. It exists so the project runs offline and
exercises the same weight-loading, convolution, and pixel-shuffle path
a trained checkpoint uses.

To use a real ESPCN checkpoint:

```bash
python scripts/export_espcn_weights.py \
    --checkpoint your_espcn_x2.pth \
    --scale 2 \
    --out data/espcn_x2.vwy
```

The `meta` tensor in the bundle selects the channel widths, kernel
sizes, scale, and activation, so the demo adapts without source edits.

## Layout

| Path | Contents |
| --- | --- |
| `src/main.vy` | decode, inference, encode, PSNR report |
| `data/sample.png` | 64x64 low-resolution input |
| `data/reference.png` | 128x128 bilinear reference |
| `data/espcn_x2.vwy` | bundled passthrough weights |
| `project.toml` | targets and dependencies |

## Learn more

- Vyne repository: <https://github.com/t2ncay/vyne>
- `vimage` manual: `vyne/modules/external/vimage/README.md`
- `vconv` manual: `vyne/modules/external/vconv/README.md`
{{#if license_spdx}}
- License: {{license_spdx}}
{{/if}}
