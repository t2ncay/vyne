---
id: vhelpers
title: Single-file Helpers
group: Helpers
icon: fa-toolbox
order: 1
description: Small standalone modules for terminal color, unit conversion, plotting, and strings.
---

# Single-file Helpers

These modules live directly under `vyne/modules/external/` rather than in their own directory. They are small convenience layers, kept for compatibility. Some were written against earlier syntax; prefer the maintained libraries when they cover the task.

Import them with an external path:

```vyne
use external "vcolors.vy";
use external "vconvert.vy";
use external "vstring.vy";
use external "vplot.vy";
```

Older code uses `use lib "vcolors.vy";`. That form is still accepted, but `use external` is the current spelling.

## vcolors

ANSI terminal styling. Exports style wrappers and a palette.

| Group | Names |
| :--- | :--- |
| Foreground | `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `gray` |
| Bright foreground | `boldBlack`, `boldRed`, `boldGreen`, `boldYellow`, `boldBlue`, `boldMagenta`, `boldCyan`, `boldWhite` |
| Background | `onBlack`, `onRed`, `onGreen`, `onYellow`, `onBlue`, `onMagenta`, `onCyan`, `onWhite` |
| Style | `bold`, `dim`, `italic`, `underline`, `blink`, `reverse`, `hidden`, `strikethrough` |
| Semantic | `success`, `error`, `warning`, `info`, `hint` |
| Custom | `rgb(r, g, b, text)`, `bgRgb(r, g, b, text)`, `rainbow(text)`, `box(text)`, `banner(text)`, `rule()` |

```vyne
use external "vcolors.vy";

out(vcolors.bold("=== training ==="));
out(vcolors.green("done"));
out(vcolors.red("failed"));
out(vcolors.rgb(120, 200, 255, "custom"));
```

The wrappers return strings with ANSI escapes. Disable color at the terminal level (`NO_COLOR`) or simply do not use the wrappers in machine-readable output.

## vconvert

Unit conversion helpers. All take and return numbers.

| Category | Functions |
| :--- | :--- |
| Temperature | `celsiusToFahrenheit`, `celsiusToKelvin`, `fahrenheitToCelsius`, `fahrenheitToKelvin`, `kelvinToCelsius`, `kelvinToFahrenheit` |
| Distance | `kmToMiles`, `milesToKm`, `kmToMeter`, `meterToKm`, `meterToCm`, `cmToMeter`, `cmToMm`, `mmToCm` |
| Time | `hoursToMin`, `minToHours`, `hoursToSec`, `secToHours`, `minToSec`, `secToMin`, `secToMs`, `msToSec` |

```vyne
use external "vconvert.vy";

out(vconvert.celsiusToFahrenheit(100.0));   # 212
out(vconvert.kmToMiles(10.0));
out(vconvert.hoursToMin(2.0));              # 120
```

## vstring

Small string helpers built on the base string methods.

| Function | Meaning |
| :--- | :--- |
| `upper(s)` | Uppercase |
| `lower(s)` | Lowercase |
| `title(s)` | Title case |
| `swapCase(s)` | Swap case per character |
| `split(s, delimiter)` | Split into an array |
| `join(arr, separator)` | Join an array into a string |

```vyne
use external "vstring.vy";

out(vstring.upper("vyne"));             # VYNE
out(vstring.title("hello world"));      # Hello World
out(vstring.join(["a", "b"], "-"));     # a-b
```

## vplot

A minimal bar-chart helper that prints bars to stdout.

```vyne
use external "vplot.vy";

vplot.bar_chart([3, 7, 2, 9], 10);
```

For real plotting, use [`vtermplot`](page:vtermplot): line, scatter, bar, histogram, heatmap, and sparkline with axes, labels, Unicode/ASCII themes, and file export.

## Compatibility note

These helpers predate the current kernel/wrapper library layout. They are not part of the numerical stack's tested surface and may use older declaration forms. New code should prefer:

- `vtermplot` instead of `vplot`;
- the built-in string methods or `vstring` instead of ad-hoc loops;
- explicit conversions instead of relying on older untyped helper signatures.

## See also

- [Library Overview](page:libs)
- [VTermPlot](page:vtermplot)
- [Built-ins & Collections](page:builtins)
