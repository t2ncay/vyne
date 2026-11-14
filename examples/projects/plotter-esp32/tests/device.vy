# Actual ESP32 adapter/lifecycle/rotation and scalar runtime regression.
use native vplot;
use native esp32;
ruleset { dynamic_casting };

fn check(ok :: Bool, label :: String) {
    if (!ok) { esp32.serial_print("VPLOT_DEVICE_FAIL "); esp32.serial_println(label); }
}
esp32.display_init();
check(float64(123)==123.0,"float conversion");
check(int64(123.5)==123,"integer conversion");
check(!false,"logical not");
direct :: Int64 := vplot.display_surface(0);
buffered :: Int64 := vplot.display_surface(8192);
p :: Int64 := vplot.new_plot("Backend checks","X","Y");
s :: Int64 := vplot.live_series(p,8,"S",vplot.rgb(56,189,248));
through i :: 0..7 -> loop { vplot.append(s,float64(i),float64(i % 3)); };
check(vplot.render(p,direct),"direct rendering");
check(vplot.render(p,buffered),"buffered rendering");
esp32.display_set_rotation(0);
check(vplot.surface_width(buffered)==240 && vplot.surface_height(buffered)==320,"rotated geometry");
check(vplot.render(p,buffered),"portrait buffer rendering");
esp32.display_set_rotation(1);
check(vplot.render_at(p,buffered,10,20,200,150),"buffer viewport");
vplot.transparent(p,true);
check(!vplot.render(p,buffered),"unsupported buffered transparency rejected");
vplot.transparent(p,false);
xs :: Array<Float64> := [0.0,1.0,2.0,3.0];
ys :: Array<Float64> := [-1.0,0.5,1.0,-0.5];
through kind :: 0..4 -> loop {
    chart :: Int64 := vplot.new_plot("Device gallery","X","Y");
    if (kind==0) { vplot.line(chart,xs,ys,"Line",vplot.rgb(56,189,248)); }
    if (kind==1) { vplot.scatter(chart,xs,ys,"Scatter",vplot.rgb(251,146,60)); }
    if (kind==2) { vplot.bars(chart,xs,ys,"Bars",vplot.rgb(74,222,128)); }
    if (kind==3) { vplot.histogram(chart,ys,4,"Histogram",vplot.rgb(192,132,252)); }
    if (kind==4) { vplot.heatmap(chart,ys,2,2,"Heatmap"); }
    check(vplot.render(chart,buffered),"all chart kinds on device");
    vplot.destroy(chart);
};
vplot.destroy(p);
vplot.destroy(direct);
vplot.destroy(buffered);
check(vplot.memory_bytes()==0,"released memory");
esp32.serial_println("VPLOT_DEVICE_PASS");
