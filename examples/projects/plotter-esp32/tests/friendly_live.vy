# Instrumented version of the short managed demo; the public starter stays short.
use external "vplot/vplot.vy";
use native esp32;
chart := vplot.live_chart("Live signals");
second := vplot.add_live_series(chart,"Modulated");
surface :: Int64 := vplot.output_surface(chart);
esp32.serial_println("VPLOT_READY");
esp32.serial_print("VPLOT_SIZE ");esp32.serial_print(vplot.surface_width(surface));esp32.serial_print("x");esp32.serial_println(vplot.surface_height(surface));
last_report :: Int64 := esp32.millis();
while true {
    seconds :: Float64 := float64(esp32.millis())/1000.0;
    vplot.push(chart,esp32.sin(seconds*2.0));
    vplot.push(second,esp32.cos(seconds*3.0)*0.5);
    now :: Int64 := esp32.millis();
    if ((now-last_report)>=2000) {
        esp32.serial_print("VPLOT_STATUS frames=");esp32.serial_print(vplot.frame_count(surface));
        esp32.serial_print(" samples=");esp32.serial_print(vplot.live_count(chart));
        esp32.serial_print(" native_bytes=");esp32.serial_print(vplot.memory_bytes());
        esp32.serial_print(" allocations=");esp32.serial_print(vplot.allocation_count());
        esp32.serial_print(" heap_free=");esp32.serial_print(esp32.free_ram());
        esp32.serial_print(" arena=");esp32.serial_println(esp32.arena_used());last_report=now;
    }
    esp32.delay(20);
}
