# V03: golden region cleanup, scratch bounds, and a checked numerical result.
region probe {
    scratch values :: Float64[3];
    values[0] = 1.0;
    values[1] = 2.0;
    values[2] = 3.0;
    total :: Float64 := values[0] + values[1] + values[2];
    if total != 6.0 { exit(1); }
    out("V03 region: " + string(total));
};
