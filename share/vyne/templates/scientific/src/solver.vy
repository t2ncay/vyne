# solver.vy - classical RK4 for the Lorenz system.
#
# State is passed as three scalars so the hot loop stays unboxed. Each
# derivative evaluation returns a three-element Array<Float64>; RK4
# combines four of them into the next state.

ruleset { dynamic_casting };

module solver;

fn :: solver deriv(
    x :: Float64,
    y :: Float64,
    z :: Float64,
    sigma :: Float64,
    rho :: Float64,
    beta :: Float64
) -> Array<Float64> {
    d :: Array<Float64> := [];
    d.push(sigma * (y - x));
    d.push(x * (rho - z) - y);
    d.push(x * y - beta * z);
    return d;
}

fn :: solver rk4_step(
    x :: Float64,
    y :: Float64,
    z :: Float64,
    dt :: Float64,
    sigma :: Float64,
    rho :: Float64,
    beta :: Float64
) -> Array<Float64> {
    k1 :: Array<Float64> := solver.deriv(x, y, z, sigma, rho, beta);

    x2 :: Float64 := x + 0.5 * dt * k1[0];
    y2 :: Float64 := y + 0.5 * dt * k1[1];
    z2 :: Float64 := z + 0.5 * dt * k1[2];
    k2 :: Array<Float64> := solver.deriv(x2, y2, z2, sigma, rho, beta);

    x3 :: Float64 := x + 0.5 * dt * k2[0];
    y3 :: Float64 := y + 0.5 * dt * k2[1];
    z3 :: Float64 := z + 0.5 * dt * k2[2];
    k3 :: Array<Float64> := solver.deriv(x3, y3, z3, sigma, rho, beta);

    x4 :: Float64 := x + dt * k3[0];
    y4 :: Float64 := y + dt * k3[1];
    z4 :: Float64 := z + dt * k3[2];
    k4 :: Array<Float64> := solver.deriv(x4, y4, z4, sigma, rho, beta);

    sixth :: Float64 := dt / 6.0;
    next :: Array<Float64> := [];
    next.push(x + sixth * (k1[0] + 2.0 * k2[0] + 2.0 * k3[0] + k4[0]));
    next.push(y + sixth * (k1[1] + 2.0 * k2[1] + 2.0 * k3[1] + k4[1]));
    next.push(z + sixth * (k1[2] + 2.0 * k2[2] + 2.0 * k3[2] + k4[2]));
    return next;
}
