# typed_arrays.vy — AVR typed-array proof.
#
# Exercises create, push, index, slice, iterate, and region rewind
# for both Array<Float64> and Array<Int64>. Confirms that the
# region-scoped arena reclaims the push-growth chain and that
# arena_used returns to 0 after each region.

use native arduino;

arduino.serial_begin(115200);
arduino.delay(200);

arduino.serial_print("=== typed arrays on Uno R3 ===\n\n");

# ── Empty literal + push ─────────────────────────────────────
# a starts as vyne_array_f64_create(0); first push grows to
# cap=8 (32 bytes of data in the arena). Subsequent pushes at
# 2..4 stay within that buffer. The whole chain is reclaimed by
# the region's rewind on exit.
region push_test {
    a :: Array<Float64> := [];
    a.push(1.0);
    a.push(2.0);
    a.push(3.0);
    a.push(4.0);

    arduino.serial_print("size  = ");
    arduino.serial_print(a.size());
    arduino.serial_print("\n");

    total :: Float64 := 0.0;
    through i :: 0..3 -> loop {
        total = total + a[i];
    };

    arduino.serial_print("sum   = ");
    arduino.serial_print(total);
    arduino.serial_print("\n");

    arduino.serial_print("mean  = ");
    arduino.serial_print(total / 4.0);
    arduino.serial_print("\n");
};

arduino.serial_print("arena after push_test = ");
arduino.serial_print(arduino.arena_used());
arduino.serial_print(" B\n\n");

# ── Fixed-size literal ───────────────────────────────────────
# [10, 20, 30, 40] lowers to vyne_array_i64_create(4) followed
# by four direct .data[i] stores. No growth.
region literal_test {
    b :: Array<Int64> := [10, 20, 30, 40];

    arduino.serial_print("fixed size = ");
    arduino.serial_print(b.size());
    arduino.serial_print("\n");

    through i :: 0..3 -> loop {
        arduino.serial_print(b[i]);
        arduino.serial_print(" ");
    };
    arduino.serial_print("\n");
};

arduino.serial_print("arena after literal_test = ");
arduino.serial_print(arduino.arena_used());
arduino.serial_print(" B\n\n");

# ── Slice ────────────────────────────────────────────────────
# c[1:3] calls vyne_array_f64_slice(&c, 1, 3); the returned
# container owns a fresh buffer of 3 elements.
region slice_test {
    c :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0];
    d :: Array<Float64> := c[1:3];

    arduino.serial_print("slice size = ");
    arduino.serial_print(d.size());
    arduino.serial_print("  first = ");
    arduino.serial_print(d[0]);
    arduino.serial_print("\n");
};

arduino.serial_print("arena after slice_test = ");
arduino.serial_print(arduino.arena_used());
arduino.serial_print(" B\n\n");

arduino.serial_print("done.\n");
