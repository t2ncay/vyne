# implicit_test.vy - self-checking regression for @implicit regions.
#
# Run: vynec examples/autodiff/implicit_test.vy
# Exits 0 on success, 1 on the first mismatch.
#
# The solver is never unrolled. `backward(root)` differentiates the
# residual at the converged root and applies the Implicit Function
# Theorem:
#
#     dz/dx_i = -(dF/dx_i) / (dF/dz)
#
# so the reverse pass costs one residual adjoint regardless of how many
# Newton iterations the primal needed.

use native vmath;

fn near(a :: Float64, b :: Float64, tol :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = -d; }
    return d <= tol;
}

# F(x, z) = z^3 + x - 1 = 0.
#
# x = 0  =>  z = 1.
# dz/dx = -1 / (3 z^2) = -1/3 at the root.
@differentiable
fn cubic_residual(x :: Float64, z :: Float64) -> Float64 {
    return z * z * z + x - 1.0;
}

x :: Float64 := 0.0;

@implicit
@bounded<64_000_000>
region solve_cubic {
    z :: Float64 := newton_solve(cubic_residual, x, 0.5);
    backward(z);

    if !near(z, 1.0, 1e-10) {
        out("FAIL cubic root: " + string(z));
        exit(1);
    }
    if !near(x.grad, -1.0 / 3.0, 1e-9) {
        out("FAIL cubic dz/dx: " + string(x.grad));
        exit(1);
    }
    if !near(z.grad, 1.0, 1e-12) {
        out("FAIL cubic root seed: " + string(z.grad));
        exit(1);
    }
};

# Same equation, but with an explicit upstream sensitivity.
# dL/dz = 2z, so dL/dx = -(2z)/(3 z^2) = -2/3 at z = 1.
@implicit
region solve_cubic_seeded {
    z :: Float64 := newton_solve(cubic_residual, x, 0.5);
    backward(z, 2.0 * z);

    if !near(x.grad, -2.0 / 3.0, 1e-9) {
        out("FAIL seeded dz/dx: " + string(x.grad));
        exit(1);
    }
};

# Two parameters. F(a, b, z) = a*z + b - 4 = 0.
#
# a = 2, b = 0  =>  z = 2.
# dz/da = -z / a = -1,  dz/db = -1 / a = -0.5.
@differentiable
fn two_param_residual(
    a :: Float64,
    b :: Float64,
    z :: Float64
) -> Float64 {
    return a * z + b - 4.0;
}

a :: Float64 := 2.0;
b :: Float64 := 0.0;

@implicit<48>
region solve_two_param {
    z :: Float64 := newton_solve(two_param_residual, a, b, 1.0);
    backward(z);

    if !near(z, 2.0, 1e-10) {
        out("FAIL two-param root: " + string(z));
        exit(1);
    }
    if !near(a.grad, -1.0, 1e-9) {
        out("FAIL two-param dz/da: " + string(a.grad));
        exit(1);
    }
    if !near(b.grad, -0.5, 1e-9) {
        out("FAIL two-param dz/db: " + string(b.grad));
        exit(1);
    }
};

# Composition: the implicit solve is an ordinary region for the memory
# model. `@bounded` accounts for the solver locals and the adjoint temps,
# and the rewind reclaims them like any other region allocation.
@implicit
@bounded<64_000_000>
region solve_repeated {
    local_x :: Float64 := 0.25;
    z :: Float64 := newton_solve(cubic_residual, local_x, 0.5);
    backward(z);

    # F(x,z)=0 at x=0.25 => z^3 = 0.75, z = 0.908560...
    if !near(z * z * z, 0.75, 1e-9) {
        out("FAIL repeated residual: " + string(z));
        exit(1);
    }
};

out("implicit_test: all checks passed");
