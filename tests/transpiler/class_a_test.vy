ruleset { dynamic_casting, warnings };

# ============================================================
# 1. Pipeline operator  |>
# ============================================================
out("=== pipeline ===");

fn double(n :: Int64) -> Int64 {
    return n * 2;
}

fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

# Simple variable pipe
out(5 |> double);

# Function call pipe: inject as first arg
out(10 |> add(3));      # add(10, 3) = 13

# Chained
out(5 |> double |> double);   # 5 -> 10 -> 20

# ============================================================
# 2. Array methods
# ============================================================
out("=== array methods ===");

arr :: Array = [3, 1, 4, 1, 5, 9, 2, 6];

out(arr.size());
out(arr.back());
out(arr.pop_front());     # removes 3
out(arr);

arr.delete(1);            # removes first occurrence of 1
out(arr);

arr.delete_at(0);         # removes element at index 0
out(arr);

arr.sort();
out(arr);

arr.place_all(0, 4);
out(arr);

arr.clear();
out(arr.size());

# ============================================================
# 3. for EVERY
# ============================================================
out("=== for every ===");

nums :: Array = [2, 4, 6, 8];
allEven = through n :: nums every { n % 2 == 0 };
out(allEven);             # true

mixed :: Array = [2, 3, 4];
allEven2 = through n :: mixed every { n % 2 == 0 };
out(allEven2);            # false

# --- Done ---
out("done");