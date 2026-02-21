module vcore;
module vmem;

fn factorial(n) {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}

out("Initial Memory:");
out(vmem.usage());

# high recursion depth
result = factorial(50); 

out("Factorial(50) Result:");
out(result);

out("Memory after recursion:");
out(vmem.usage());