ruleset { dynamic_casting };

try {
    x = 10;
    if x > 5 {
        throw "Too big!";
    }
} catch (err) {
    out("Error: " + err);
} finally {
    out("Cleanup!");
}