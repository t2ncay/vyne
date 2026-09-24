ruleset { warnings };

x :: Int64   = 25;
y :: Float64 = float64(x);

x_size :: Int64 = sizeof(x);

out("x : " + string(x));
out("y : " + string(y));
out("x_size : " + string(x_size));