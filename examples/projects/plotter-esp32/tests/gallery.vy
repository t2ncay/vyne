# Host gallery: vynec --run -o build/gallery tests/gallery.vy
use external "vplot/Image.vy";
ruleset { dynamic_casting };

fn check(ok :: Bool, message :: String) {
    if (!ok) { out(vplot.last_error()); throw message; }
}

xs :: Array<Float64> := [0.0,1.0,2.0,3.0,4.0,5.0];
ys :: Array<Float64> := [0.0,0.8,0.2,-0.7,0.5,0.0];
zs :: Array<Float64> := [0.3,-0.4,0.7,0.2,-0.3,0.6];
surface :: Int64 := vplot.image_surface(400, 300);
line :: Int64 := vplot.new_plot("Polished lines", "Time (s)", "Value");
vplot.line(line,xs,ys,"Channel A",vplot.rgb(56,189,248));
vplot.line(line,xs,zs,"Channel B",vplot.rgb(251,146,60));
vplot.thickness(line,2);
check(vplot.render(line,surface),"line render failed");
check(vplot.save_png(surface,"build/plots/line.png"),"line PNG failed");
check(vplot.destroy(line),"line destroy failed");

scatter :: Int64 := vplot.new_plot("Scatter", "X", "Y");
vplot.scatter(scatter,xs,ys,"Observations",vplot.rgb(244,114,182));
vplot.marker_size(scatter,4);
vplot.theme(scatter,vplot.LIGHT);
check(vplot.render(scatter,surface),"scatter render failed");
check(vplot.save_png(surface,"build/plots/scatter.png"),"scatter PNG failed");
vplot.destroy(scatter);

bars :: Int64 := vplot.new_plot("Signed bars", "Bin", "Amount");
vplot.bars(bars,xs,ys,"Values",vplot.rgb(74,222,128));
check(vplot.render(bars,surface),"bar render failed");
check(vplot.save_png(surface,"build/plots/bars.png"),"bar PNG failed");
vplot.destroy(bars);

histogram :: Int64 := vplot.new_plot("Histogram", "Value", "Count");
vplot.histogram(histogram,ys,5,"Distribution",vplot.rgb(192,132,252));
check(vplot.render(histogram,surface),"histogram render failed");
check(vplot.save_png(surface,"build/plots/histogram.png"),"histogram PNG failed");
vplot.destroy(histogram);

values :: Array<Float64> := [-1.0,-0.5,0.0,0.5,1.0,0.4,0.2,-0.8,0.7,0.1,-0.2,-0.4];
heatmap :: Int64 := vplot.new_plot("Diverging heatmap", "Column", "Row");
vplot.heatmap(heatmap,values,4,3,"Intensity");
vplot.autoscale(heatmap,0.0,0.0,0.0);
check(vplot.render(heatmap,surface),"heatmap render failed");
check(vplot.save_png(surface,"build/plots/heatmap.png"),"heatmap PNG failed");
vplot.destroy(heatmap);
vplot.destroy(surface);
check(vplot.memory_bytes()==0,"native plotting memory leaked");
out("VPLOT_GALLERY_PASS");
