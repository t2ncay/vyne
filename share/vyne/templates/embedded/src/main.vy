# {{name}} — {{board_label}}
# One-time setup: vynec hardware setup
# Compile: vynec build
# Compile, upload, reset and start: vynec run
# Optional serial monitor: vynec run --monitor

ruleset { dynamic_casting };
{{#if esp32}}
use native esp32;
{{#if hw950}}
esp32.display_init(0);
esp32.display_clear(0);
esp32.display_text(12, 24, "{{name}}", 65535, 2);
esp32.touch_screen_init();
esp32.serial_println("{{name}}: HW-950 ready");
@bounded<4096> region screen {
    through i :: 0..1000 -> loop {
        if (esp32.touch_screen_pressed()) {
            esp32.display_circle_filled(esp32.touch_screen_x(), esp32.touch_screen_y(), 3, 2016);
        }
        esp32.delay(10);
    };
};
{{/if}}
{{#if generic_esp32}}
esp32.serial_println("{{name}}: ESP32 ready");
@bounded<4096> region hello {
    through i :: 0..5 -> loop {
        esp32.serial_println("Hello from ESP32");
        esp32.delay(200);
    };
};
{{/if}}
{{/if}}
{{#if uno}}
use native arduino;
arduino.serial_begin({{hardware_baud}});
arduino.pin_mode(13, 1);
arduino.serial_println("{{name}}: starting blink");
@bounded<512> region blink {
    through i :: 0..5 -> loop {
        arduino.digital_write(13, 1);
        arduino.delay(200);
        arduino.digital_write(13, 0);
        arduino.delay(200);
    };
};
arduino.serial_println("{{name}}: 5 blinks complete");
{{/if}}
