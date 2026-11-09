# vsignal/vsignal.vy - facade. `use external "vsignal/vsignal.vy"` pulls
# the core filter surface: series construction, moving average,
# exponential, FIR, median, biquad IIR, windows, and resampling.
#
# Spectrum helpers live in `Spectral.vy` and depend on `vfft`. They are
# deliberately not imported here so a program that only needs filtering
# does not pull in the transform machinery. Import
# `vsignal/Spectral.vy` explicitly when you need them.

use "Types.vy";
use "Constructors.vy";
use "Kernels.vy";
use "Iir.vy";
use "Fir.vy";
use "Windows.vy";
use "Resample.vy";

ruleset { dynamic_casting };

module vsignal;

deploy vsignal;
