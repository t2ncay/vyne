n :: Int64 = 40;
a = 0;
b = 1;
i = 0;

out("Calculating Fibonacci(40)...");

while (i < n) {
    temp = a;
    a = b;
    b = temp + b;
    i++;
}

out("Result:");
out(a);