ruleset { dynamic_casting };

x :: Int64 = 31;
x_ref :: Int64& = x;
x_ref = 69; # should this work like omggg????
out(x); # prints 69