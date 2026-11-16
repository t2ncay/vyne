# V02: numerical output and region behavior agree across host C profiles.
region probe {
    scratch values :: Float64[4];
    values[0] = 1.0;
    values[1] = 2.0;
    values[2] = 3.0;
    values[3] = 4.0;
    total :: Float64 := values[0] + values[1] + values[2] + values[3];
    if total != 10.0 { exit(9); }
};
out("V02 checksum: 10");
