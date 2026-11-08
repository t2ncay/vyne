# vi2c/vi2c.vy - facade. `use external "vi2c/vi2c.vy"` pulls the typed
# I2C surface. MCU-only: it lowers to the `arduino.i2c_*` bindings, which
# exist only on the AVR target.

use "Ops.vy";

ruleset { dynamic_casting };

module vi2c;

deploy vi2c;
