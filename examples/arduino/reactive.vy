use native arduino;

# The Uno has exactly one built-in LED. It is on pin 13.
# Pin 13 is not PWM, so use digital_write + patterns, not analog_write.
led_pin :: Int64 := 13;

arduino.pin_mode(led_pin, 1);

# Boot indicator: three fast blinks. No blinks = flash didn't land.
through i :: 0..2 -> loop {
    arduino.digital_write(led_pin, 1);
    arduino.delay(120);
    arduino.digital_write(led_pin, 0);
    arduino.delay(120);
};
arduino.delay(400);

max_drift :: Int64 := 0;
last_beat :: Int64 := 0;

through iteration :: 1..1000000 -> loop {
    # ---- the entire claim of the memory model, in ten lines ----
    before :: Int64 := arduino.arena_used();

    region proof {
        scratch buffer :: Float64[64];
        through n :: 0..63 -> loop {
            buffer[n] = (n * 1.5);
        };
    };

    after :: Int64 := arduino.arena_used();
    drift :: Int64 := after - before;
    if drift > max_drift { max_drift = drift; }
    # ---- end of the claim ----

    if max_drift > 0 {
        # SOS forever. If you see this, the memory model is broken
        # and the compiler has a bug. Report it — this is the
        # failure mode the demo exists to catch.
        through i :: 0..2 -> loop {
            arduino.digital_write(led_pin, 1);
            arduino.delay(150);
            arduino.digital_write(led_pin, 0);
            arduino.delay(150);
        };
        arduino.delay(200);
        through i :: 0..2 -> loop {
            arduino.digital_write(led_pin, 1);
            arduino.delay(450);
            arduino.digital_write(led_pin, 0);
            arduino.delay(150);
        };
        arduino.delay(200);
        through i :: 0..2 -> loop {
            arduino.digital_write(led_pin, 1);
            arduino.delay(150);
            arduino.digital_write(led_pin, 0);
            arduino.delay(150);
        };
        arduino.delay(1000);
    } else {
        # Heartbeat: double-blink once per second.
        # Proves the loop is running AND the region rewinds every frame.
        now :: Int64 := arduino.millis();
        if now - last_beat >= 1000 {
            last_beat = now;
            arduino.digital_write(led_pin, 1);
            arduino.delay(50);
            arduino.digital_write(led_pin, 0);
            arduino.delay(100);
            arduino.digital_write(led_pin, 1);
            arduino.delay(50);
            arduino.digital_write(led_pin, 0);
        }
    }
};
