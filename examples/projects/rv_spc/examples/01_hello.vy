# 01_hello.vy - serial hello.
#
#   vynec --arduino uno examples/01_hello.vy

use native arduino;

ruleset { dynamic_casting };

arduino.serial_begin(9600);
arduino.serial_println("Hello from rv_spc!");
