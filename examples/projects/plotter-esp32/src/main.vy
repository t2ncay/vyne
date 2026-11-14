# A managed live chart: configured display, colors, buffering and redraws are automatic.
use external "vplot/vplot.vy";
use native esp32;

chart := vplot.live_chart("Live signals");
second := vplot.add_live_series(chart, "Modulated");
while true {
    seconds :: Float64 := float64(esp32.millis()) / 1000.0;
    vplot.push(chart, esp32.sin(seconds * 2.0));
    vplot.push(second, esp32.cos(seconds * 3.0) * 0.5);
    esp32.delay(20);
}
