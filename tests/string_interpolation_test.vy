ruleset { dynamic_casting };

name = "Vyne";
out("Hello {name}!");

out("2 + 2 = {2 + 2}");

msg = "test";
out("Upper: {msg.uppercase()}");

a = 10;
b = 20;
out("{a} + {b} = {a + b}");

out("Just a normal string");
out("Escaped {{ not interpolated }}");