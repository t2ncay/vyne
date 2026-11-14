---
id: vplot
title: Universal Plotting
group: Standard Library
icon: fa-chart-line
order: 25
description: Colorful charts and bounded live updates on ESP32 displays and vimage surfaces.
---

# vplot — universal plotting

`vplot` shares chart geometry, layout, fonts and colors between ESP32 displays
and raster/vimage output. Line, scatter, signed bar, histogram and heatmap
charts support automatic/fixed linear ranges, major/minor grids, labels,
legends, dark/light themes and dashboard viewports.

```vyne
use external "vplot/vplot.vy";
plot :: Int64 := vplot.new_plot("Signals", "Time (s)", "Value");
series :: Int64 := vplot.live_series(plot, 256, "Channel", vplot.rgb(56,189,248));
vplot.autoscale(plot, 0.05, 0.25, 10.0);
vplot.append(series, 1.0, 0.8);
vplot.render(plot, surface);
```

## Choose a surface

ESP32 attaches to an initialized logical display:

```vyne
use native esp32;
esp32.display_init();
surface :: Int64 := vplot.display_surface(16384);
```

Hardware configuration stays in `hardware.toml` and the ESP32 runtime. The
adapter uses a reusable RGB565 band buffer, preserving the same image geometry
after color quantization. Pass `0` for direct drawing. Rotation changes refresh
the surface dimensions; no driver/pin/mirror/calibration settings are changed.

Host images use an optional adapter:

```vyne
use external "vplot/Image.vy";
surface :: Int64 := vplot.image_surface(640,360);
vplot.render(plot,surface);
vplot.save_png(surface,"plot.png");
```

`to_image(surface)` explicitly converts to an RGB Float64 `vimage.Image`.
`from_image(image)` borrows grayscale/RGB/RGBA pixels for chart overlays. Keep
borrowed buffers alive and unresized. Image encoding/conversion is separate
from allocation-free rendering; output directories must exist.

## Live updates and ownership

Fixed-capacity native rings overwrite their oldest sample. `append`, `render`,
`set_data` within capacity and histogram/heatmap updates reuse reserved storage.
Visible-window autoscaling and hysteresis stabilize axes; first/min/max/last
column reduction preserves spikes in dense monotonic lines. NaNs/infinities
break segments, and geometry is clipped before rasterization.

Plots own their series. Destroy plots and independently owned surfaces with
`vplot.destroy`. Generation checks reject stale/wrong-kind handles. Borrowed
Float64 line views are available through `line_view`; they redraw automatically
and require stable caller-owned buffers. Native state survives region rewinds.

Mutations/render return `Bool`, constructors return `0` on failure, and
`last_error()` explains errors. `rgb`/`pixel_rgb` use `-1` as their error sentinel.
`memory_bytes`, `peak_bytes` and `allocation_count` expose owned native storage.
The application owns sampling/refresh timing and synchronization.

## Test project

`examples/projects/plotter-esp32/` contains the live HW-950 dashboard, retained
touch example, five PNG galleries and all plotting acceptance sources.

```sh
python examples/projects/plotter-esp32/tests/run.py
python examples/projects/plotter-esp32/tests/run.py --esp32 --port COM5 --seconds 60
```

The host suite covers C/C++/sanitizer behavior, native API/types, errors, memory,
PNG decoding and 128 band/viewport/chart/theme parity cases. The physical suite
checks direct/buffered rendering, rotation, every chart type and live metrics.

See the full API at `vyne/modules/external/vplot/README.md` and the exact coverage
catalog at `examples/projects/plotter-esp32/tests/TESTS.md`. Shared pixel/span and
presentation callbacks make additional adapters possible without duplicating
chart algorithms. Current compiled targets are desktop and ESP32.
