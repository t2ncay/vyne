# vstat_test.vy - behavioural checks for the vstat library.
#
# Run compiled:
#   ./vynec --compile examples/test/vstat_test.vy
#   ./examples/test/vstat_test

use external "vstat/vstat.vy";

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn check(name :: String, cond :: Bool) {
    if cond {
        out("PASS " + name);
    } else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vstat ===");

data :: Array<Float64> := [2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0];
s :: vstat.Series := vstat.Series(data, data.size());

check("sum", close(vstat.sum(s), 40.0));
check("mean", close(vstat.mean(s), 5.0));
check("minimum", close(vstat.minimum(s), 2.0));
check("maximum", close(vstat.maximum(s), 9.0));
check("range", close(vstat.range(s), 7.0));
check("variance-pop", close(vstat.variance(s, false), 4.0));
check("variance-sample", close(vstat.variance(s, true), 32.0 / 7.0));
check("stddev-pop", close(vstat.stddev(s, false), 2.0));
check("median", close(vstat.median(s), 4.5));
check("quantile-25", close(vstat.quantile(s, 0.25), 4.0));
check("quantile-75", close(vstat.quantile(s, 0.75), 5.5));
check("percentile-50", close(vstat.percentile(s, 50.0), 4.5));
check("iqr", close(vstat.iqr(s), 1.5));
check("mad", close(vstat.mad(s), 0.5));
check("mode", close(vstat.mode(s), 4.0));

weights :: Array<Float64> := [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0];
check("weighted_mean",
      close(vstat.weighted_mean(s, vstat.Series(weights, 8)), 5.0));

summ :: vstat.Summary := vstat.summary(s);
check("summary-n", summ.n == 8);
check("summary-mean", close(summ.mean, 5.0));
check("summary-variance", close(summ.variance, 4.0));
check("summary-stddev", close(summ.stddev, 2.0));
check("summary-median", close(summ.median, 4.5));

ordered :: vstat.Series := vstat.sorted_copy(s);
check("sorted-first", close(ordered.data[0], 2.0));
check("sorted-last", close(ordered.data[7], 9.0));

zs :: vstat.Series := vstat.zscore(s);
check("zscore-mean", close(vstat.mean(zs), 0.0));
check("zscore-stddev", close(vstat.stddev(zs, false), 1.0));

norm :: vstat.Series := vstat.minmax_normalize(s);
check("minmax-min", close(vstat.minimum(norm), 0.0));
check("minmax-max", close(vstat.maximum(norm), 1.0));

cs :: vstat.Series := vstat.cumulative_sum(s);
check("cumulative_sum", close(cs.data[7], 40.0));

diff :: vstat.Series := vstat.difference(s);
check("difference-len", diff.n == 7);
check("difference-first", close(diff.data[0], 2.0));

ranks :: vstat.Series := vstat.rank(s);
check("rank-sum", close(vstat.sum(ranks), 36.0));

check("covariance-self", close(vstat.covariance(s, s, false), 4.0));
check("pearson-self", close(vstat.pearson(s, s), 1.0));
check("spearman-self", close(vstat.spearman(s, s), 1.0));

# y = 2x + 3
xs :: Array<Float64> := [];
ys :: Array<Float64> := [];
through i :: 0..7 -> loop {
    xs.push(float64(i));
    ys.push(2.0 * float64(i) + 3.0);
};
sx :: vstat.Series := vstat.Series(xs, 8);
sy :: vstat.Series := vstat.Series(ys, 8);
fit :: vstat.LinearFit := vstat.linear(sx, sy);
check("linear-slope", close(fit.slope, 2.0));
check("linear-intercept", close(fit.intercept, 3.0));
check("linear-r2", close(fit.r2, 1.0));
check("linear-predict", close(vstat.predict(fit, 10.0), 23.0));
resid :: vstat.Series := vstat.residuals(sx, sy, fit);
check("linear-residuals", close(vstat.k_abs_sum(resid.data, resid.n), 0.0));

# Intercept column + one predictor: y = 3 + 2x
X :: Array<Float64> := [1.0, 0.0, 1.0, 1.0, 1.0, 2.0, 1.0, 3.0];
Y :: Array<Float64> := [3.0, 5.0, 7.0, 9.0];
beta :: vstat.Series := vstat.multiple(X, vstat.Series(Y, 4), 4, 2);
check("multiple-interpret", close(beta.data[0], 3.0));
check("multiple-slope", close(beta.data[1], 2.0));
beta_r :: vstat.Series := vstat.ridge(X, vstat.Series(Y, 4), 4, 2, 0.0);
check("ridge-zero-alpha", close(beta_r.data[1], 2.0));

h :: vstat.Histogram := vstat.histogram_range(s, 4, 2.0, 10.0);
check("hist-bins", h.counts.size() == 4);
check("hist-count-0", h.counts[0] == 1);
check("hist-count-1", h.counts[1] == 5);
check("hist-count-2", h.counts[2] == 1);
check("hist-count-3", h.counts[3] == 1);

dens :: Array<Float64> := vstat.density(h);
check("density-len", dens.size() == 4);
check("density-integral",
      close(vstat.sum(vstat.Series(dens, 4)) * 2.0, 1.0));

cdf :: Array<Float64> := vstat.cdf(h);
check("cdf-last", close(cdf[3], 1.0));
check("empirical-cdf", close(vstat.empirical_cdf(s, 5.0), 0.75));

probs :: Array<Float64> := [0.25, 0.75];
qs :: Array<Float64> := vstat.quantiles(s, vstat.Series(probs, 2));
check("quantiles-25", close(qs[0], 4.0));
check("quantiles-75", close(qs[1], 5.5));

grid :: Array<Float64> := [5.0];
kde :: vstat.Series := vstat.kde(s, 1.0, vstat.Series(grid, 1));
check("kde-positive", kde.data[0] > 0.0);

wf :: vstat.Welford := vstat.welford_new();
through i :: 0..7 -> loop { wf = vstat.welford_add(wf, data[i]); };
check("welford-n", wf.n == 8);
check("welford-mean", close(wf.mean, 5.0));
check("welford-variance", close(vstat.welford_variance(wf, false), 4.0));

wc :: vstat.WelfordCov := vstat.wcov_new();
through i :: 0..7 -> loop { wc = vstat.wcov_add(wc, data[i], data[i]); };
check("wcov-covariance", close(vstat.wcov_covariance(wc, false), 4.0));
check("wcov-pearson", close(vstat.wcov_pearson(wc), 1.0));

boxed :: vstat.Series := vstat.from_array(data);
check("from_array", close(vstat.mean(boxed), 5.0));
check("to_array-len", vstat.to_array(boxed).size() == 8);

if failures[0] > 0 {
    out("vstat: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vstat: all checks passed");
