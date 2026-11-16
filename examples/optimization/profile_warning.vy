# V02: a real generated-C warning must be visible in the diagnostic profile.
region probe {
    intentionally_unused :: Int64 := 41;
    out("V02 warning fixture reached");
};
