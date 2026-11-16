# Fixed-channel observed Wi-Fi frames/s. Send Q over serial to stop and release.
use external "vplot/vplot.vy";
use native esp32;

const CHANNEL :: Int64 := 6;
const SAMPLE_MS :: Int64 := 500;
esp32.serial_begin(115200);
esp32.serial_print("SNIFF_HEAP before="); esp32.serial_println(esp32.free_ram());
if !esp32.wifi_sniff_start(CHANNEL) {
    esp32.serial_print("SNIFF_ERROR start="); esp32.serial_println(esp32.wifi_sniff_error());
    esp32.delay(250); exit(1);
}
esp32.serial_print("SNIFF_HEAP after_wifi="); esp32.serial_println(esp32.free_ram());
chart :: Int64 := vplot.chart("Observed Wi-Fi - CH " + string(CHANNEL));
management :: Int64 := vplot.live_series(chart, 512, "Management", vplot.rgb(56, 189, 248));
data :: Int64 := vplot.live_series(chart, 512, "Data", vplot.rgb(251, 146, 60));
control :: Int64 := vplot.live_series(chart, 512, "Control", vplot.rgb(244, 114, 182));
if chart == 0 || management == 0 || data == 0 || control == 0 {
    esp32.wifi_sniff_stop(); vplot.close(chart);
    esp32.serial_println("SNIFF_ERROR chart allocation"); esp32.delay(250); exit(2);
}
vplot.labels(chart, "Time (s)", "Frames/s");
vplot.legend(chart, true);
vplot.autoscale(chart, 0.05, 0.25, 10.0);
if !vplot.show(chart) {
    esp32.wifi_sniff_stop(); vplot.close(chart);
    esp32.serial_println("SNIFF_ERROR display"); esp32.delay(250); exit(3);
}
surface :: Int64 := vplot.output_surface(chart);
esp32.serial_print("SNIFF_READY channel="); esp32.serial_println(CHANNEL);
seconds :: Float64 := 0.0;
reports :: Int64 := 0;
running :: Bool := true;
while running {
    esp32.delay(SAMPLE_MS);
    if esp32.serial_available() > 0 {
        key :: Int64 := esp32.serial_read();
        if key == 81 || key == 113 { running = false; }
    }
    if running && esp32.wifi_sniff_sample() {
        elapsed :: Float64 := float64(esp32.wifi_sniff_elapsed_us()) / 1000000.0;
        seconds = seconds + elapsed;
        mgmt :: Int64 := esp32.wifi_sniff_management();
        frames :: Int64 := esp32.wifi_sniff_data();
        ctrl :: Int64 := esp32.wifi_sniff_control();
        if esp32.wifi_sniff_saturated() {
            esp32.serial_println("SNIFF_ERROR saturated interval"); running = false;
        } else {
            # Append all traces before a single redraw: same timestamp/interval.
            if !vplot.append(management, seconds, float64(mgmt) / elapsed) { running = false; }
            if !vplot.append(data, seconds, float64(frames) / elapsed) { running = false; }
            if !vplot.append(control, seconds, float64(ctrl) / elapsed) { running = false; }
            if !vplot.show(chart) { running = false; }
            if !running { esp32.serial_println("SNIFF_ERROR plotting"); }
            reports = reports + 1;
            if reports % 4 == 0 {
                esp32.serial_print("SNIFF_STATUS channel="); esp32.serial_print(CHANNEL);
                esp32.serial_print(" elapsed_us="); esp32.serial_print(esp32.wifi_sniff_elapsed_us());
                esp32.serial_print(" management="); esp32.serial_print(mgmt);
                esp32.serial_print(" data="); esp32.serial_print(frames);
                esp32.serial_print(" control="); esp32.serial_print(ctrl);
                esp32.serial_print(" frames="); esp32.serial_print(vplot.frame_count(surface));
                esp32.serial_print(" samples="); esp32.serial_print(vplot.series_count(management));
                esp32.serial_print(" native_bytes="); esp32.serial_print(vplot.memory_bytes());
                esp32.serial_print(" allocations="); esp32.serial_print(vplot.allocation_count());
                esp32.serial_print(" heap_free="); esp32.serial_print(esp32.free_ram());
                esp32.serial_print(" arena="); esp32.serial_println(esp32.arena_used());
            }
        }
    } else {
        if running {
            esp32.serial_print("SNIFF_ERROR sample="); esp32.serial_println(esp32.wifi_sniff_error());
            running = false;
        }
    }
}
if !esp32.wifi_sniff_stop() {
    esp32.serial_print("SNIFF_ERROR stop="); esp32.serial_println(esp32.wifi_sniff_error());
}
vplot.close(chart);
esp32.serial_print("SNIFF_STOPPED native_bytes="); esp32.serial_print(vplot.memory_bytes());
esp32.serial_print(" heap_free="); esp32.serial_println(esp32.free_ram());
