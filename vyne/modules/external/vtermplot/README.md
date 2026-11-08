# vtermplot — terminal plotting for Vyne

`vtermplot` is a matplotlib-flavoured plotting layer that renders into the
terminal. It supports line, scatter, bar, histogram, heatmap, and sparkline
visualisations with Unicode glyphs (ASCII fallback), optional ANSI colour, axes,
labels, and file export.

```vyne
use external "vtermplot/vtermplot.vy";

xs :: Array<Float64> := [0.0, 1.0, 2.0, 3.0, 4.0, 5.0];
ys :: Array<Float64> := [0.0, 1.0, 4.0, 9.0, 16.0, 25.0];

vtermplot.show(vtermplot.plot_line(xs, ys, 60, 18, "y = x^2"));
```

The renderer is intentionally dependency-free: it draws with `out` and strings,
uses `vcolors` for optional ANSI styling, and uses `vfs` only for `save`.

## Concepts

A `Plot` owns a flat `width × height` grid of small integer codes:

| Code | Meaning |
| --- | --- |
| 0 | empty |
| 1 | scatter point |
| 2 | line |
| 3 | bar |
| 5..13 | heatmap level |

`SeriesData` carries a copied `Array<Float64>` pair plus a label and marker code.
You can build a plot imperatively (create, draw several series, render) or use a
one-call convenience function.

## API

### Canvas

| Function | Result |
| --- | --- |
| `new_plot(width, height, title, x_label, y_label)` | empty `Plot` |
| `set_unicode(plot, on)` | toggle Unicode/ASCII glyphs |
| `set_color(plot, on)` | toggle ANSI colour in `show` |
| `set_grid(plot, on)` | toggle axes/border |
| `render(plot)` | `String` containing the whole plot |
| `show(plot)` | print the plot (applies colour when enabled) |
| `save(plot, path)` | write `render(plot)` to a file via `vfs` |
| `series(xs, ys, label, marker)` | copy two boxed arrays into `SeriesData` |

### Line and scatter

| Function | Result |
| --- | --- |
| `line(plot, xs, ys, marker)` | draw a polyline |
| `line_series(plot, series)` | draw a `SeriesData` polyline |
| `scatter(plot, xs, ys, marker)` | draw points |
| `scatter_series(plot, series)` | draw `SeriesData` points |
| `plot_line(xs, ys, width, height, title)` | create, draw, render a line plot |
| `plot_scatter(xs, ys, width, height, title)` | create, draw, render a scatter plot |
| `plot_line_series(series, width, height, title)` | one-call line plot from a series |

### Bars and histograms

| Function | Result |
| --- | --- |
| `bars(plot, ys, marker)` | vertical bars from `y=0`, x = 0..n-1 |
| `bars_series(plot, series)` | draw `SeriesData` bars |
| `hist(plot, xs, bins, marker)` | equal-width histogram |
| `plot_bars(ys, width, height, title)` | one-call bar chart |
| `plot_hist(xs, bins, width, height, title)` | one-call histogram |
| `bars_series_plot(series, width, height, title)` | one-call bar chart from a series |

### Heatmaps

| Function | Result |
| --- | --- |
| `heatmap(plot, data, rows, cols)` | map a row-major matrix to 9 luminance levels |
| `plot_heatmap(data, rows, cols, width, height, title)` | one-call heatmap |

The matrix is resampled to the plot's `width × height`; the minimum maps to the
emptiest level and the maximum to a full block.

### Sparklines

| Function | Result |
| --- | --- |
| `sparkline(xs)` | Unicode one-line chart (`▁▂▃▄▅▆▇█`) |
| `sparkline_ascii(xs)` | ASCII one-line chart (`_.-=+*#@`) |
| `sparkline_series(series, unicode)` | sparkline from a `SeriesData` |

## Examples

### Multiple series on one canvas

```vyne
p :: vtermplot.Plot := vtermplot.new_plot(70, 20, "signals", "t", "amplitude");
a :: vtermplot.SeriesData := vtermplot.series(t, sin_t, "sin", 2);
b :: vtermplot.SeriesData := vtermplot.series(t, cos_t, "cos", 2);
vtermplot.line_series(p, a);
vtermplot.line_series(p, b);
vtermplot.show(p);
```

### Histogram

```vyne
vtermplot.show(vtermplot.plot_hist(samples, 24, 60, 16, "sample distribution"));
```

### Heatmap and sparkline

```vyne
vtermplot.show(vtermplot.plot_heatmap(corr, 16, 16, 32, 16, "correlation"));
out("trend: " + vtermplot.sparkline(series_values));
```

### ASCII / colour / file

```vyne
p :: vtermplot.Plot := vtermplot.new_plot(50, 14, "portable", "", "");
vtermplot.set_unicode(p, false);
vtermplot.set_color(p, true);
vtermplot.line(p, xs, ys, 2);
vtermplot.save(p, "plot.txt");
vtermplot.show(p);
```

## Notes

- Coordinates are padded to the plot bounds; equal min/max spans collapse to the
  centre line.
- `line` and `scatter` update the plot's shared bounds, so several series share
  one coordinate frame.
- `bars`/`hist` always include `y=0` in their bounds so the baseline is visible.
- Unicode glyphs are wide characters in some terminals; if alignment matters,
  call `set_unicode(p, false)` for the ASCII theme.
- `save` writes the uncoloured rendering (ANSI codes are only applied by
  `show`).
- AVR is not a target: terminal plotting depends on desktop `out` and `vfs`.

## Tests and benchmark

```bash
./vynec --compile examples/test/vtermplot_test.vy
./examples/test/vtermplot_test

./vynec --compile examples/benchmark/vtermplot_bench.vy
./examples/benchmark/vtermplot_bench
```
