ruleset { dynamic_casting };

out("--- Vyne C-Transpiler Start ---");

x = 10;
y = 20;
result = x + y * 2;

out("Result of 10 + 20 * 2:");
out(result);

count = 0;
out("Starting while loop...");
while (count < 5) {
    out(count);
    count = count + 1;
}

if (result > 40) {
    out("Result is greater than 40");
} else {
    out("Result is smaller than 40");
}

out("--- Test Finished ---");