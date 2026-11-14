# 02_bounded.vy - a region with an explicit SRAM ceiling.
#
#   vynec --arduino uno examples/02_bounded.vy

use native arduino;

ruleset { dynamic_casting };

arduino.serial_begin(9600);

@bounded<512> region sample_buffer {
    xs :: Array<Float64> := [];
    xs.place_all(1.0, 32);
    arduino.serial_println("buffer[31] = " + string(xs[31]));
};
