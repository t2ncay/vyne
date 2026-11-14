---
id: vplot
title: Universal Plotting
group: Data & Plotting
icon: fa-chart-line
order: 40
description: Colorful charts and bounded live updates on ESP32 displays and vimage surfaces.
---

# vplot — universal plotting

## A chart in a few lines

```vyne
use external "vplot/vplot.vy";
xs := [0,1,2,3];
ys := [22.0,23.4,21.8,24.1];
vplot.show_line(xs,ys,"Temperature");
```

Initialize with `--kind embedded --preset hw950 --libraries vplot --port COM5`,
run `vynec hardware setup` once, then `vynec run`. The starter uses this short
API. Configured display initialization, theme, colors, ranges, reusable buffer
selection and temporary cleanup are automatic. Existing display rotation is
preserved; no surface or memory budget is needed in the quick-start code.

On desktop, use `vplot.save_line(xs,ys,"Temperature","temperature.png")`.
Equivalent quick helpers cover scatter, bars, histograms and heatmaps.

```vyne title="save-line.vy"
use external "vplot/vplot.vy";
xs := [0, 1, 2, 3];
ys := [22.0, 23.4, 21.8, 24.1];
vplot.save_line(xs, ys, "Temperature", "temperature.png");
```

## Managed charts and live values

```vyne
chart := vplot.chart("Temperature");
vplot.add_line(chart,xs,ys,"Sensor");
vplot.labels(chart,"Time (s)","C");
vplot.show(chart);
vplot.close(chart);
```

`show` presents on ESP32 or prepares a 640x360 image on desktop; `save(chart,path)`
exports PNG and creates parent directories. Managed surfaces are reused and
released with the chart. Existing styling functions still apply.

`chart := vplot.live_chart("Temperature")` shows initial axes and reserves a
512-sample ring. Submit values with `vplot.push(chart,value)`; timestamps and
best-effort 100 ms redraw throttling are automatic. Sampling remains application
controlled. `add_live_series(chart,label)` returns an automatically colored
additional series usable with `push`. Use `push_xy` for explicit coordinates.
Native objects and Vyne arena storage do not grow during updates.

## Advanced surfaces

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

See the [full API](https://github.com/t2ncay/vyne/blob/master/vyne/modules/external/vplot/README.md)
and the [coverage catalog](https://github.com/t2ncay/vyne/blob/master/examples/projects/plotter-esp32/tests/TESTS.md).
Shared pixel/span and
presentation callbacks make additional adapters possible without duplicating
chart algorithms. Current compiled targets are desktop and ESP32.

## See also

- [ESP32 & Hardware](page:esp32) and [Projects & Tooling](page:projects)
- [vtermplot](page:vtermplot) for terminal output
