# vplot — universal charts and bounded live updates

Select your board once, install the hardware dependencies, and write a chart:

```vyne
use external "vplot/vplot.vy";
xs := [0,1,2,3];
ys := [22.0,23.4,21.8,24.1];
vplot.show_line(xs,ys,"Temperature");
```

```sh
vynec init screen --name screen --kind embedded --preset hw950 --libraries vplot --port COM5
cd screen
vynec hardware setup
vynec run
```

`show_line` initializes the configured display once, uses the gallery's theme,
colors and automatic ranges, chooses a reusable buffer, presents the chart and
releases temporary objects. Existing application-selected display rotation is
preserved. Buffer selection tries 16/8/4 KiB, then direct drawing if necessary.

The same portable C11 engine serves all outputs. Native aliases are supported
with `use native vplot as chart;`. Explicit surfaces are an advanced option.

## Quick charts and managed objects

| Display helper | Host PNG helper |
| --- | --- |
| `show_line(xs,ys,title)` | `save_line(xs,ys,title,path)` |
| `show_scatter(xs,ys,title)` | `save_scatter(xs,ys,title,path)` |
| `show_bars(xs,ys,title)` | `save_bars(xs,ys,title,path)` |
| `show_histogram(values,title)` | `save_histogram(values,title,path)` |
| `show_heatmap(values,columns,rows,title)` | `save_heatmap(values,columns,rows,title,path)` |

Quick helpers manage creation and cleanup. Histogram bin count is automatic.
Host helpers create parent output directories and encode PNG directly from
the native RGB buffer, without an intermediate Float64 image.

For styling/multiple series, retain a chart:

```vyne
chart := vplot.chart("Temperature");
vplot.add_line(chart,xs,ys,"Sensor");
vplot.labels(chart,"Time (s)","C");
vplot.show(chart);                 # ESP32: show on the configured display
vplot.close(chart);                # releases the chart and its managed surface
```

On desktop, `show(chart)` prepares a managed 640x360 image. Use
`save(chart,"temperature.png")` to export it; it does not open an interactive
window. Managed charts support the existing theme/grid/legend/limits functions.
`output_surface(chart)` provides advanced access; the chart owns that surface.

## Simple live updates

```vyne
chart := vplot.live_chart("Temperature");
# Whenever a sample arrives:
vplot.push(chart,value);
```

`live_chart` shows initial axes and reserves a 512-sample ring. `push` records
time since chart creation and redraws at most once per requested 100 ms interval.
It does not control sample acquisition or promise a fixed frame rate. Use
`push_xy(chart,x,value)` for explicit coordinates. `add_live_series(chart,label)`
adds an automatically colored series; pass that series to `push`/`push_xy`.
`live_count(chart)` inspects the default ring. Creation reserves storage;
updates allocate neither native objects nor Vyne arena storage.

The live clock handles the ESP32 millisecond counter wrapping. Managed
`show`/`save` reuse their surface, and both `close(chart)` and existing
`destroy(chart)` release it. Quick transient charts require no explicit cleanup.

## Advanced: explicit surfaces

### ESP32

```vyne
use native esp32;
esp32.display_init();
surface :: Int64 := vplot.display_surface(16384);
```

The adapter attaches to the already initialized logical display. It never
selects pins, drivers, rotation, mirrors or touch calibration. Those belong to
`hardware.toml` and the ESP32 runtime. The verified HW-950 setup is used by the
demo project. Native rendering uses RGB565; the API uses RGB888 colors.

`display_surface(buffer_bytes)` reserves a reusable RGB565 band buffer. `0`
selects direct drawing; a positive budget must hold at least one full-width row
and must not exceed 65536 bytes. Allocation failure returns handle `0`.
The default demo uses 16 KiB, rather than a full-screen sprite. Logical display
dimensions are refreshed after application-requested rotation changes.

### Images and PNG

```vyne
use external "vplot/Image.vy";
surface :: Int64 := vplot.image_surface(640,360);
vplot.render(p,surface);
vplot.save_png(surface,"plot.png");
image :: vimage.Image := vplot.to_image(surface);
```

Owned image surfaces use compact RGB888 storage. `to_image` explicitly allocates
an RGB Float64 `vimage.Image`; it is an export/conversion operation, not part of
the allocation-free redraw path. `save_png` uses the existing vimage encoder;
the output directory must exist. Other vimage-supported encodings are available
by passing the converted image to `vimage.save`.

`vplot.from_image(image)` borrows an existing grayscale/RGB/RGBA Float64 image.
Keep its pixels alive and do not resize them while the surface exists. Rendering
writes opaque RGB colors (and alpha 1 for RGBA). `transparent(p,true)` preserves
the existing background and draws chart elements over it. Buffered ESP32
surfaces reject this mode; direct display and image surfaces support it.

## API

Handles are `Int64`, generation-checked and distinguished internally as plots,
series or surfaces. Constructors return `0` on failure. Mutations/render return
`Bool`; `last_error()` reports the cause. `rgb` and `pixel_rgb` return `-1` on
failure so black (`0`) remains a valid color.

| Call | Purpose |
| --- | --- |
| `new_plot(title,x_label,y_label)` | Create a plot; strings are copied, maximum 95 bytes each |
| `live_series(plot,capacity,label,color)` | Attach an owned rolling line series |
| `line(plot,xs,ys,label,color)` | Attach copied line data |
| `line_view(plot,xs,ys,label,color)` | Attach borrowed Float64 arrays without copying |
| `scatter(plot,xs,ys,label,color)` | Attach copied scatter data |
| `bars(plot,xs,ys,label,color)` | Attach signed bars with zero baseline |
| `bar_width(series,width)` | Set bar width in data coordinates |
| `histogram(plot,values,bins,label,color)` | Finite samples, inclusive final bin; non-finite values are skipped |
| `histogram_update(series,values)` | Update existing histogram buffers without allocation |
| `heatmap(plot,values,columns,rows,label)` | Row-major grid, row zero at the top; viridis/diverging color scale and colorbar |
| `heatmap_update(series,values)` | Update the same grid shape without allocation |
| `append(series,x,y)` | Append/overwrite the oldest sample in an owned ring |
| `clear(series)` | Clear samples and return a borrowed series to its owned buffer |
| `set_data(series,xs,ys)` | Replace copied data within reserved capacity |
| `series_count(series)`, `sample_x/y(series,index)` | Inspect logical oldest-to-newest samples |
| `limits(plot,xmin,xmax,ymin,ymax)` | Fixed finite, increasing bounds with representable spans |
| `autoscale(plot,padding,hysteresis,window)` | Automatic ranges; window is in x-axis units, `0` means all data |
| `theme(plot,DARK/LIGHT)` | Built-in contrasting themes |
| `grid(plot,bool)`, `minor_grid(plot,bool)` | Major and optional five-way minor subdivisions |
| `legend(plot,bool)` | Labeled series swatches; long names truncate, excess rows report `+N` |
| `thickness(plot,1..7)`, `marker_size(plot,1..8)` | Line and scatter appearance |
| `decimation(plot,bool)` | Ordered per-column first/min/max/last reduction for dense monotonic lines |
| `render(plot,surface)` | Render across the surface |
| `render_at(plot,surface,x,y,width,height)` | Dashboard viewport, minimum 32x32 |
| `invalidate(plot)` | Force redraw after external changes |
| `surface_width/height(surface)`, `frame_count(surface)` | Surface geometry and completed renders |
| `memory_bytes()`, `peak_bytes()`, `allocation_count()` | Owned native plotting storage statistics |
| `destroy(plot_or_surface)` | Explicitly release resources |

Copied arrays accept boxed numeric, Float64 and Int64 arrays (also Float32
arrays on desktop); x/y
lengths must match. Borrowed line views require Float64 arrays and automatically
redraw, because external writes cannot be revision-tracked. Their buffers must
remain live/unresized until their plot is destroyed or the series is cleared.

Plots own their series. Destroy the plot to release all series and invalidate
their handles; surfaces are independently owned and must also be destroyed.
Objects and native buffers survive Vyne region rewinds. Calls are single-threaded;
the application owns acquisition, synchronization and refresh scheduling.

## Rendering behavior and resource limits

- Empty/all-non-finite data uses default ranges. Constant data gets a nonzero span.
- NaN/infinity break line segments; offscreen geometry is clipped before integer rasterization.
- Autoscaling a scrolling window uses only visible samples for the y range.
- Hysteresis retains nearby previous y bounds to avoid jitter.
- Dense monotonic lines preserve narrow spikes; unordered lines keep input order.
- Unchanged owned plots can reuse their last render. Borrowed data/images render again.
- Compact viewports reduce decorations; shared authored 5x7 bitmap glyphs keep
  baseline text metrics consistent. Letters, digits and common punctuation are
  supported; other byte values use a question-mark glyph.
- RGB fills are opaque. Scales are linear in this release.
- Default limits: 64 live handles, 8 series per plot, 1048576 points per series,
  surface dimensions up to 4096. The first two limits can be configured with
  `VPLOT_MAX_HANDLES` / `VPLOT_MAX_SERIES` at compilation.
- Native allocation failures are reported. Creation, export and destruction may
  allocate/free; steady-state append/render/update reuse reserved buffers.
- Image/export functions target desktop builds; display surfaces target ESP32.
  The compiled native backend is required (`vynec build/run`, not the interpreter).

The C adapter contract is `VPCanvas` pixel/span callbacks plus `VPSurface`
presentation/refresh hooks in `runtime/vplot/vplot_core.h`. New backends implement
that contract without changing chart algorithms. Logical clipping is separate
from output-band clipping so image and RGB565 band rendering are pixel-equivalent
after color quantization. SVG/window/terminal adapters can build on this contract.

## Tests and live project

See `examples/projects/plotter-esp32/README.md` and `tests/TESTS.md`. The suite
covers C/C++ portability, sanitizer checks, chart/viewport/theme/band parity,
native API/types, errors, borrowed data, live memory stability and decoded PNGs.
The original standalone `vplot.vy` text bar helper retains its API/output with
modern declaration/loop syntax;
the new universal API is imported from `vplot/vplot.vy`.
