# vspi/vspi.vy - facade. `use external "vspi/vspi.vy"` pulls the typed
# SPI surface. MCU-only: it lowers to the `arduino.spi_*` bindings, which
# exist only on the AVR target.

use "Ops.vy";

ruleset { dynamic_casting };

module vspi;

deploy vspi;
