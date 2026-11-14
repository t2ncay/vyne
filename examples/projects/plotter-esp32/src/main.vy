# vplot live dashboard. Hardware setup stays in hardware.toml and the ESP32
# runtime; the plotting core only sees the corrected logical display surface.
use external "vplot/vplot.vy";
use native esp32;
ruleset { dynamic_casting };

esp32.display_init();
surface :: Int64 := vplot.display_surface(16384);
plot :: Int64 := vplot.new_plot("Live signals", "Time (s)", "Value");
channel_a :: Int64 := vplot.live_series(plot, 512, "Sine", vplot.rgb(56, 189, 248));
channel_b :: Int64 := vplot.live_series(plot, 512, "Modulated", vplot.rgb(251, 146, 60));
vplot.autoscale(plot, 0.05, 0.25, 10.0);
vplot.thickness(plot, 2);
if (surface == 0 || plot == 0 || channel_a == 0 || channel_b == 0) {
    esp32.serial_println(vplot.last_error());
    while true { esp32.delay(1000); }
}
vplot.render(plot, surface);
esp32.serial_println("VPLOT_READY");
esp32.serial_print("VPLOT_SIZE ");
esp32.serial_print(vplot.surface_width(surface));
esp32.serial_print("x");
esp32.serial_println(vplot.surface_height(surface));
last_sample :: Int64 := esp32.millis();
last_frame :: Int64 := last_sample;
last_report :: Int64 := last_sample;

while true {
    @bounded<4096> region tick {
        now :: Int64 := esp32.millis();
        seconds :: Float64 := float64(now) / 1000.0;
        if ((now - last_sample) >= 20) {
            a :: Float64 := esp32.sin(seconds * 2.0);
            b :: Float64 := esp32.cos(seconds * 3.0) * (0.5 + 0.2 * esp32.sin(seconds));
            vplot.append(channel_a, seconds, a);
            vplot.append(channel_b, seconds, b);
            last_sample = now;
        }
        if ((now - last_frame) >= 100) {
            if (!vplot.render(plot, surface)) { esp32.serial_println(vplot.last_error()); }
            last_frame = now;
        }
        if ((now - last_report) >= 2000) {
            esp32.serial_print("VPLOT_STATUS frames=");
            esp32.serial_print(vplot.frame_count(surface));
            esp32.serial_print(" samples=");
            esp32.serial_print(vplot.series_count(channel_a));
            esp32.serial_print(" native_bytes=");
            esp32.serial_print(vplot.memory_bytes());
            esp32.serial_print(" allocations=");
            esp32.serial_print(vplot.allocation_count());
            esp32.serial_print(" heap_free=");
            esp32.serial_print(esp32.free_ram());
            esp32.serial_print(" arena=");
            esp32.serial_println(esp32.arena_used());
            last_report = now;
        }
    };
    esp32.delay(1);
}
