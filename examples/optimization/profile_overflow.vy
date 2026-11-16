# V02: intentional signed overflow proves that emitted C is instrumented.
# This is a detector fixture for C11, not a new language overflow policy.
maximum :: Int64 := 9223372036854775807;
one :: Int64 := 1;
out("V02 UBSan probe entered");
out(maximum + one);
out("V02 UBSan probe escaped (BUG)");
