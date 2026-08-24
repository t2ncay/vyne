ruleset { dynamic_casting };

# Test 1: Basic try-catch with error
try {
    out("Inside try block");
    throw "Something went wrong!";
    out("This should not be printed");
}
catch (err) {
    out("Caught error: " + string(err));
}

# Test 2: Try-catch without error
try {
    out("Inside try block (no error)");
    x = 42;
}
catch (err) {
    out("This should not be printed");
}

out("Value of x: " + string(x));

# Test 3: Try with finally only (no catch)
try {
    out("Inside try with finally");
    y = 100;
}
finally {
    out("Finally block executed");
}

out("Value of y: " + string(y));

# Test 4: Try-catch-finally
try {
    out("Inside try-catch-finally");
    z = 1 / 0;  # This will throw
    out("This should not print");
}
catch (e) {
    out("Caught division error: " + string(e));
}
finally {
    out("Finally block executed after catch");
}

out("Done with all tests");