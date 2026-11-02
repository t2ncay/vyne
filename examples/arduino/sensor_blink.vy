use native arduino;

led_pin :: Int64 := 13;
sensor_pin :: Int64 := 0; # A0

arduino.pin_mode(led_pin, 1);

through i :: 1..10000 -> loop {
    # Read the analog value (0 to 1023)
    sensor_val :: Int64 := arduino.analog_read(sensor_pin);
    
    # Map the sensor value to a delay (e.g., 100ms to 1123ms)
    # If the light is bright (high value), blink slower.
    wait_ms :: Int64 := 100 + sensor_val;
    
    arduino.digital_write(led_pin, 1);
    arduino.delay(wait_ms);
    
    arduino.digital_write(led_pin, 0);
    arduino.delay(wait_ms);
};
