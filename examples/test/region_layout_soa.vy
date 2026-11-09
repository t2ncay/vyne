# region_layout_soa.vy — @layout<soa> struct scratch.
#
# Run compiled (regions and layout lowering are compiled-backend features):
#   ./vynec --compile examples/test/region_layout_soa.vy
#   ./examples/test/region_layout_soa
#
# Verifies:
#   - @layout<soa> composes with a memory policy (@bounded here)
#   - scratch p :: Particle[N] lowers to one packed array per field
#   - particles[i].field reads and writes address the field arrays
#   - a multi-index SoA scratch (Particle[rows, cols]) flattens correctly
#   - the values survive a full loop and the region rewind releases storage

interface Particle {
    x :: Float64,
    y :: Float64,
    z :: Float64,
    mass :: Float64,
}

fn soa_runtime_sum(n :: Int64) -> Float64 {
    total :: Float64 := 0.0;
    @layout<soa> region dyn {
        scratch p :: Particle[n];
        through i :: 0..n - 1 -> loop { p[i].x = float64(i); };
        through i :: 0..n - 1 -> loop { total = total + p[i].x; };
    };
    return total;
}

out("=== @layout<soa> test ===");

# --- [1] 1-D SoA: 10_000 particles, 32 bytes each -------------------
@layout<soa> @bounded<400_000> region physics {
    scratch particles :: Particle[10_000];

    through i :: 0..9_999 -> loop {
        particles[i].x = float64(i);
        particles[i].y = float64(i) * 2.0;
        particles[i].z = float64(i) * 3.0;
        particles[i].mass = 1.0;
    };

    sum_x :: Float64 := 0.0;
    sum_mass :: Float64 := 0.0;
    through i :: 0..9_999 -> loop {
        sum_x = sum_x + particles[i].x;
        sum_mass = sum_mass + particles[i].mass;
    };

    # sum(0..9999) = 49_995_000
    out(sum_x == 49_995_000.0);
    out(sum_mass == 10_000.0);
    out(particles[0].x == 0.0);
    out(particles[9_999].z == 29_997.0);

    # A primitive scratch in the same region keeps the normal layout.
    scratch tmp :: Float64[8];
    through i :: 0..7 -> loop { tmp[i] = float64(i); };
    out(tmp[7] == 7.0);
};

# --- [2] 2-D SoA: row-major flattening ------------------------------
@layout<soa> region grid {
    scratch cells :: Particle[64, 32];

    through r :: 0..63 -> loop {
        through c :: 0..31 -> loop {
            cells[r, c].x = float64(r);
            cells[r, c].y = float64(c);
            cells[r, c].mass = 0.5;
        };
    };

    out(cells[0, 0].x == 0.0);
    out(cells[63, 31].x == 63.0);
    out(cells[63, 31].y == 31.0);
    out(cells[17, 5].mass == 0.5);
};

# --- [3] runtime-length SoA (VLA per field) -------------------------
out(soa_runtime_sum(10) == 45.0);
out(soa_runtime_sum(100) == 4_950.0);

out("=== done ===");
