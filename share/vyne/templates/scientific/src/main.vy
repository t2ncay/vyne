# main.vy - Lorenz attractor integration and a finite-time Lyapunov estimate.
#
# Run from the project root:
#   vynec run src/main.vy
#
# The whole integration lives inside one @bounded region. Every RK4
# evaluation allocates four small derivative arrays; 16 MiB is enough for
# the 10000-step run with headroom. 10000 steps (100 time units) gives a
# finite-time Lyapunov estimate close to the asymptotic Lorenz value.

use native vmath;
use external "vtermplot/vtermplot.vy";
use "solver.vy";

ruleset { dynamic_casting };

out("=== {{name}} - Lorenz attractor ===");
out("");

@bounded<16777216> region simulation {
    sigma :: Float64 := 10.0;
    rho :: Float64 := 28.0;
    beta :: Float64 := 8.0 / 3.0;
    dt :: Float64 := 0.01;
    steps :: Int64 := 10000;
    d0 :: Float64 := 0.0000001;

    x :: Float64 := 1.0;
    y :: Float64 := 1.0;
    z :: Float64 := 1.0;

    # A second trajectory starts d0 away in x. Renormalising after every
    # step keeps the separation estimate in the linear regime.
    px :: Float64 := x + d0;
    py :: Float64 := y;
    pz :: Float64 := z;

    xs :: Array<Float64> := [];
    ys :: Array<Float64> := [];
    sum_log :: Float64 := 0.0;

    through s :: 0..steps-1 -> loop {
        n :: Array<Float64> := solver.rk4_step(x, y, z, dt, sigma, rho, beta);
        np :: Array<Float64> := solver.rk4_step(
            px, py, pz, dt, sigma, rho, beta
        );

        dx :: Float64 := np[0] - n[0];
        dy :: Float64 := np[1] - n[1];
        dz :: Float64 := np[2] - n[2];
        dist :: Float64 := vmath.sqrt(dx * dx + dy * dy + dz * dz);

        if dist > 0.0 {
            sum_log = sum_log + vmath.log(dist / d0);
            scale :: Float64 := d0 / dist;
            px = n[0] + dx * scale;
            py = n[1] + dy * scale;
            pz = n[2] + dz * scale;
        }

        x = n[0];
        y = n[1];
        z = n[2];

        xs.push(x);
        ys.push(y);
    };

    lyap :: Float64 := sum_log / (float64(steps) * dt);

    out("integrator : RK4, dt=" + string(dt) + ", steps=" + string(steps));
    out("final state: x=" + string(x) + " y=" + string(y) +
        " z=" + string(z));
    out("Lyapunov   : ~" + string(lyap) + " (known Lorenz value ~0.906)");
    out("");
    out(vtermplot.plot_line(xs, ys, 64, 18, "Lorenz attractor (x-y)"));
};
