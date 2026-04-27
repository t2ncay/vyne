ruleset { dynamic_casting };

fn fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

out("Fibonacci(30) result:");
out(fib(20));