# Host managed API needs only the primary vplot import, not an image adapter.
use external "vplot/vplot.vy";

xs := [0,1,2,3];
ys := [22.0,23.4,21.8,24.1];
chart := vplot.chart("Temperature");
vplot.add_line(chart,xs,ys,"Sensor");
vplot.labels(chart,"Time","C");
if (!vplot.save(chart,"build/quick_plots/chart.png")) { throw "managed save failed"; }
vplot.close(chart);
if (!vplot.save_line(xs,ys,"Line","build/quick_plots/line.png")) { throw "quick line failed"; }
if (!vplot.save_scatter(xs,ys,"Scatter","build/quick_plots/scatter.png")) { throw "quick scatter failed"; }
if (!vplot.save_bars(xs,ys,"Bars","build/quick_plots/bars.png")) { throw "quick bars failed"; }
if (!vplot.save_histogram(ys,"Histogram","build/quick_plots/histogram.png")) { throw "quick histogram failed"; }
if (!vplot.save_heatmap(ys,2,2,"Heatmap","build/quick_plots/heatmap.png")) { throw "quick heatmap failed"; }
if (vplot.memory_bytes()!=0) { throw "quick chart cleanup failed"; }
out("VPLOT_FRIENDLY_PASS");
