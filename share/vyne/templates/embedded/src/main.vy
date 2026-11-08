# main.vy - blink the built-in LED on an Arduino Uno.
#
# Build:
#   vynec --arduino uno src/main.vy
# Upload (after installing the Arduino AVR core):
#   vynec --arduino uno --flash --port COM3 src/main.vy

use native arduino;

ruleset { dynamic_casting };

arduino.serial_begin(9600);
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
