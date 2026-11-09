# perf_hotpaths_test.vy - regression coverage for the hot-path fixes in
# perf/top5-compiler-runtime.
#
# Run compiled:
#   ./vynec --compile examples/test/perf_hotpaths_test.vy
#   ./examples/test/perf_hotpaths_test
#
# Covers:
#   - string trim/uppercase/lowercase values and lengths (single-copy buffers)
#   - map set/get/overwrite/missing-key probes (interned-hash reuse)
#   - literal loop bounds (inline codegen) and captured variable bounds
#   - a long string literal (bulk lexer scan)

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond {
        out("PASS " + name);
    } else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== perf hot paths ===");

# --- strings ---------------------------------------------------------
s :: String := "  Hello World  ";
t :: String := s.trim();
u :: String := t.uppercase();
v :: String := u.lowercase();

check("trim-value", t == "Hello World");
check("trim-length", t.size() == 11);
check("uppercase-value", u == "HELLO WORLD");
check("uppercase-length", u.size() == 11);
check("lowercase-value", v == "hello world");
check("lowercase-length", v.size() == 11);

abc :: String := "abcdef";
slice :: String := abc[1:3];
check("string-slice", slice == "bc");

interpolated :: String := "v={1 + 2}";
check("interpolated-value", interpolated == "v=3");

arr :: Array := [1, 2, 3];
check("array-string", string(arr) == "[1, 2, 3]");

# --- maps ------------------------------------------------------------
m :: Map := map();
m["alpha"] = 1;
m["beta"] = 2;
m["alpha"] = 3;

check("map-overwrite", m["alpha"] == 3);
check("map-second-key", m["beta"] == 2);
check("map-missing", m["gamma"] == null);
check("map-missing-has", m.has("gamma") == false);
m.delete("gamma");
check("map-missing-delete", m.size() == 2);

missingKey :: String := "no" + "pe";
check("map-missing-dynamic", m[missingKey] == null);

through i :: 0..63 -> loop {
    m["k" + string(i)] = i;
};

total :: Int64 := 0;
through i :: 0..63 -> loop {
    total = total + m["k" + string(i)];
};
check("map-growth-sum", total == 2016);

# --- loop bounds -----------------------------------------------------
literal_sum :: Int64 := 0;
through i :: 0..100 -> loop {
    literal_sum = literal_sum + i;
};
check("literal-bound-sum", literal_sum == 5050);

# A variable bound is still captured at loop entry, even if the body
# mutates the variable. Only literals are inlined by the emitter.
bound :: Int64 := 5;
captured :: Int64 := 0;
through i :: 0..bound-1 -> loop {
    bound = 100;
    captured = captured + i;
};
check("variable-bound-captured", captured == 10);

# --- long literal ----------------------------------------------------
long :: String := "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
check("long-literal-length", long.size() == 260);

out("=== done ===");
if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1);
}
