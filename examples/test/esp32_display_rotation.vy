# Display initialization regression: configured default must not override an
# explicit rotation. Build/run from an initialized HW-950 project with:
#   vynec run <repo>/examples/test/esp32_display_rotation.vy --port COM5
# Or pass this source's absolute path to `vynec run` from the project.
use native esp32;

fn report_rotation(label :: String) {
    esp32.serial_print(label);
    esp32.serial_print(" rotation=");
    esp32.serial_print(esp32.display_rotation());
    esp32.serial_print(" width=");
    esp32.serial_print(esp32.display_width());
    esp32.serial_print(" height=");
    esp32.serial_println(esp32.display_height());
}

esp32.display_init();
report_rotation("DEFAULT");
esp32.display_init(0);
report_rotation("EXPLICIT_0");
esp32.display_init(1);
report_rotation("EXPLICIT_1");
esp32.display_init(2);
report_rotation("EXPLICIT_2");
esp32.display_init(3);
report_rotation("EXPLICIT_3");
esp32.display_init();
report_rotation("DEFAULT_AGAIN");
esp32.serial_println("display rotation: DONE");
