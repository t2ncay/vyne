# V03: golden native scalar emission and interpreter/native output agreement.
answer :: Int64 := 6 * 7;
if answer != 42 { exit(1); }
out("V03 scalar: " + string(answer));
