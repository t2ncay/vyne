use native arduino;

# Pin 13 is the built-in LED on most Arduino boards
led_pin :: Int64 := 13;

# Setup: runs once
arduino.pin_mode(led_pin, 1); # 1 = OUTPUT

# Main loop
through i :: 1..10000 -> loop {
    arduino.digital_write(led_pin, 1); # Turn LED ON
    arduino.delay(1000);               # Wait 1 second
    
    arduino.digital_write(led_pin, 0); # Turn LED OFF
    arduino.delay(1000);               # Wait 1 second
};
