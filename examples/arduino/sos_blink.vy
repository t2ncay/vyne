use native arduino;

led_pin :: Int64 := 13; # Built-in LED

arduino.pin_mode(led_pin, 1); # 1 = OUTPUT
arduino.serial_begin(115200);

out("Vyne SOS Test starting...");

through cycle :: 1..5 -> loop {
    
    # S (...) - 3 short blinks
    through i :: 1..3 -> loop {
        arduino.digital_write(led_pin, 1);
        arduino.delay(200);
        arduino.digital_write(led_pin, 0);
        arduino.delay(200);
    };
    
    arduino.delay(500);
    
    # O (---) - 3 long blinks
    through i :: 1..3 -> loop {
        arduino.digital_write(led_pin, 1);
        arduino.delay(600);
        arduino.digital_write(led_pin, 0);
        arduino.delay(200);
    };
    
    arduino.delay(500);
    
    # S (...) - 3 short blinks
    through i :: 1..3 -> loop {
        arduino.digital_write(led_pin, 1);
        arduino.delay(200);
        arduino.digital_write(led_pin, 0);
        arduino.delay(200);
    };
    
    # Wait 2 seconds before repeating
    arduino.delay(2000);
    
    out("SOS cycle completed");
};
