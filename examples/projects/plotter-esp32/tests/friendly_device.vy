# Only test diagnostics import esp32; the quick-chart application does not.
use external "vplot/vplot.vy";
use native esp32;
xs := [0,1,2,3];
ys := [22.0,23.4,21.8,24.1];
if (!vplot.show_line(xs,ys,"Temperature")) { esp32.serial_println("VPLOT_FRIENDLY_FAIL line"); }
if (esp32.display_rotation()!=1) { esp32.serial_println("VPLOT_FRIENDLY_FAIL default rotation"); }
if (vplot.memory_bytes()!=0) { esp32.serial_println("VPLOT_FRIENDLY_FAIL quick cleanup"); }
esp32.display_set_rotation(0);
if (!vplot.show_scatter(xs,ys,"Scatter")) { esp32.serial_println("VPLOT_FRIENDLY_FAIL scatter"); }
if (esp32.display_rotation()!=0) { esp32.serial_println("VPLOT_FRIENDLY_FAIL rotation overwritten"); }
vplot.show_bars(xs,ys,"Bars");
vplot.show_histogram(ys,"Histogram");
vplot.show_heatmap(ys,2,2,"Heatmap");
esp32.display_set_rotation(1);
chart := vplot.live_chart("Managed live");
if (chart==0) { esp32.serial_println("VPLOT_FRIENDLY_FAIL creation"); }
surface :: Int64 := vplot.output_surface(chart);
baseline :: Int64 := vplot.allocation_count();
bytes :: Int64 := vplot.memory_bytes();
initial_frames :: Int64 := vplot.frame_count(surface);
started :: Int64 := esp32.millis();
through i :: 0..100 -> loop { vplot.push(chart,float64(i%10));esp32.delay(5); };
if (vplot.allocation_count()!=baseline||vplot.memory_bytes()!=bytes) { esp32.serial_println("VPLOT_FRIENDLY_FAIL allocation"); }
if ((vplot.frame_count(surface)-initial_frames)*100 > (esp32.millis()-started)+100) { esp32.serial_println("VPLOT_FRIENDLY_FAIL throttle"); }
vplot.close(chart);
if (vplot.memory_bytes()!=0) { esp32.serial_println("VPLOT_FRIENDLY_FAIL managed cleanup"); }
esp32.serial_println("VPLOT_FRIENDLY_DEVICE_PASS");
