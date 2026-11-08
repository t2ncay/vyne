# vstat — statistics for Vyne

`vstat` is the descriptive-statistics and least-squares layer for Vyne. It is
written in Vyne, in the same kernel/wrapper pattern as `vlin`: `Kernels.vy`
holds the unboxed loops, the other leaves validate and allocate, and
`vstat.vy` re-exports the whole surface.

```vyne
use external "vstat/vstat.vy";

data :: Array<Float64> := [2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0];
s :: vstat.Series := vstat.Series(data, data.size());

out(vstat.mean(s));          # 5.0
out(vstat.median(s));        # 4.5
out(vstat.stddev(s, false)); # 2.0
out(vstat.quantile(s, 0.75));# 5.5
```

## Why a `Series`

A `Series` pairs a typed `Array<Float64>` with its length:

```vyne
interface Series {
    data :: Array<Float64>,
    n    :: Int64,
}
```

Passing a `Series` keeps the data pointer and the length together, which is what
lets a wrapper call the `k_*` kernels with unboxed `double*` arguments. If you
already have a typed array, use `vstat.Series(data, data.size())` (no copy). If
you have a boxed `Array`, `vstat.from_array(xs)` copies it once and returns a
`Series`.

## API

### Construction and conversion

| Function | Result |
| --- | --- |
| `vstat.Series(data, n)` | interface constructor; no copy |
| `vstat.from_array(xs :: Array)` | copy a boxed array into a `Series` |
| `vstat.to_array(s)` | copy a `Series` into `Array<Float64>` |
| `vstat.sorted_copy(s)` | ascending copy |
| `vstat.rank(s)` | average ranks, ties shared |

### Reductions

| Function | Result |
| --- | --- |
| `sum(s)` | sum |
| `mean(s)` | arithmetic mean |
| `minimum(s)` / `maximum(s)` | extrema |
| `range(s)` | `maximum - minimum` |
| `variance(s, sample)` | population or sample variance |
| `stddev(s, sample)` | square root of variance |
| `median(s)` | 50th percentile |
| `quantile(s, q)` | linearly interpolated quantile, `q` in `[0,1]` |
| `percentile(s, p)` | `quantile(s, p/100)` |
| `iqr(s)` | interquartile range |
| `mad(s)` | median absolute deviation |
| `mode(s)` | most frequent value (first in sorted order on ties) |
| `weighted_mean(s, weights)` | weighted arithmetic mean |
| `summary(s)` | `Summary { n, mean, variance, stddev, minimum, maximum, median }` |

### Derived series

`zscore`, `standardize` (alias), `minmax_normalize`, `abs_series`,
`cumulative_sum`, `difference`.

### Association

| Function | Result |
| --- | --- |
| `covariance(a, b, sample)` | covariance |
| `pearson(a, b)` | Pearson correlation |
| `spearman(a, b)` | Spearman rank correlation |
| `autocorrelation(s, lag)` | correlation of `s` with its lagged self |
| `cross_correlation(a, b, lag)` | correlation of `a` with lagged `b` |
| `r_squared(y, y_hat)` | coefficient of determination |

### Regression

| Function | Result |
| --- | --- |
| `linear(x, y)` | `LinearFit { slope, intercept, r2 }` |
| `predict(fit, x)` | `slope*x + intercept` |
| `residuals(x, y, fit)` | observed minus predicted `Series` |
| `multiple(X, y, rows, cols)` | least-squares coefficients as a `Series` |
| `ridge(X, y, rows, cols, alpha)` | ridge coefficients |
| `predict_row(beta, row)` | dot product of coefficients and one design row |

`X` is row-major and must already include an intercept column if the model needs
one. The normal equations use a local partial-pivot Gaussian solve; `vdecomp`
will provide the full LU/QR/eigen surface.

### Image and error metrics

| Function | Result |
| --- | --- |
| `mse(a, b)` | mean squared error |
| `mae(a, b)` | mean absolute error |
| `psnr(a, b, max_value)` | peak signal-to-noise ratio in dB; identical inputs return 100 dB |
| `ssim(a, b, width, height, channels)` | mean SSIM over HWC `[0,1]` buffers using an 8x8 uniform window |

`psnr` takes the data range explicitly (`1.0` for normalised images,
`255.0` for 8-bit buffers). `ssim` expects `width * height * channels`
samples in HWC order, the same layout `vimage.Image` uses. The window is
uniform rather than Gaussian; state that when reporting numbers against
published SSIM values.

### Histograms and density

| Function | Result |
| --- | --- |
| `histogram(s, bins)` | equal-width `Histogram { edges, counts }` |
| `histogram_range(s, bins, lo, hi)` | histogram over an explicit range |
| `bin_edges(h)` / `bin_counts(h)` | the histogram arrays |
| `density(h)` | count / (total · width) per bin |
| `cdf(h)` | cumulative counts / total |
| `empirical_cdf(s, x)` | fraction of samples `<= x` |
| `quantiles(s, ps)` | one quantile per probability in `ps` |
| `kde(s, bandwidth, grid)` | Gaussian kernel density estimate on `grid` |

### Streaming (one pass)

| Function | Result |
| --- | --- |
| `welford_new()` | empty Welford state |
| `welford_add(state, x)` | state + one sample |
| `welford_variance(state, sample)` | variance |
| `welford_stddev(state, sample)` | standard deviation |
| `welford_merge(a, b)` | merge two chunk states |
| `welford_from_series(s)` | fold a whole series |
| `wcov_new()` | empty bivariate state |
| `wcov_add(state, x, y)` | state + one pair |
| `wcov_covariance(state, sample)` | covariance |
| `wcov_variance_x/y(state, sample)` | per-variable variance |
| `wcov_pearson(state)` | Pearson correlation |
| `wcov_from_series(x, y)` | fold two series |

## Semantics and limits

- Empty inputs throw for reductions that have no defined value (`mean`,
  `median`, `minimum`, …). `sum` of an empty `Series` is `0`.
- `variance`, `stddev`, `covariance`, and the Welford variants take a `sample`
  flag. `false` is population, `true` is sample (n−1). Sample variants need at
  least two values.
- `median`, `quantile`, `iqr`, and `mode` copy and sort the input; the input is
  never mutated.
- `spearman` uses O(n²) average ranking. It is correct for ties and fine for the
  usual few-thousand-sample analysis; use `vstat.rank` + `pearson` directly if
  you need a different rank strategy.
- `kde` uses an isotropic Gaussian kernel with the caller's bandwidth.

## Tests and benchmark

```bash
./vynec --compile examples/test/vstat_test.vy
./examples/test/vstat_test

./vynec --compile examples/benchmark/vstat_bench.vy
./examples/benchmark/vstat_bench
```
