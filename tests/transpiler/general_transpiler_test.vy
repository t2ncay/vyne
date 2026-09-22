ruleset { warnings, verbose, dynamic_casting };

# ============================================================
# 1. LITERALS
# ============================================================
out("=== literals ===");
out(42);              # 42
out(3.14);            # 3.14
out("hello");         # hello
out(true);            # true
out(false);           # false
out(null);            # null

# ============================================================
# 2. ARITHMETIC AND COMPARISON
# ============================================================
out("=== arithmetic ===");
out(10 + 3);          # 13
out(10 - 3);          # 7
out(10 * 3);          # 30
out(10 / 3);          # 3
out(10 % 3);          # 1
out(10 // 3);         # 3
out(10 ** 2);         # 100.0
out(7.5 / 2.0);       # 3.75
out(-5);              # -5
out(!true);           # false

out("=== comparison ===");
out(5 == 5);          # true
out(5 != 3);          # true
out(5 > 3);           # true
out(5 < 3);           # false
out(5 >= 5);          # true
out(5 <= 3);          # false
out(true && false);   # false
out(true || false);   # true

# ============================================================
# 3. CONTROL FLOW
# ============================================================
out("=== if/while ===");
x :: Int64 = 10;
if x > 5 {
    out("big");
} else {
    out("small");
}
# big

i :: Int64 = 0;
while i < 3 {
    out(i);
    i = i + 1;
}
# 0
# 1
# 2

# ============================================================
# 4. FUNCTIONS AND RECURSION
# ============================================================
out("=== functions ===");

fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

fn fact(n :: Int64) -> Int64 {
    if n <= 1 {
        return 1;
    }
    return n * fact(n - 1);
}

out(add(3, 4));       # 7
out(fact(5));         # 120

# ============================================================
# 5. ARRAYS
# ============================================================
out("=== arrays ===");
arr :: Array = [10, 20, 30, 40];
out(arr);             # [10, 20, 30, 40]
out(arr[1]);          # 20
out(arr.size());      # 4

arr.push(50);
out(arr);             # [10, 20, 30, 40, 50]

out(arr.pop());       # 50
out(arr.back());      # 40
out(arr.pop_front()); # 10
out(arr);             # [20, 30, 40]

arr.reverse();
out(arr);             # [40, 30, 20]

arr.sort();
out(arr);             # [20, 30, 40]

arr.delete_at(0);
out(arr);             # [30, 40]

arr.delete(30);
out(arr);             # [40]

arr.place_all(0, 3);
out(arr);             # [0, 0, 0]

arr.clear();
out(arr.size());      # 0

# ============================================================
# 6. MAPS
# ============================================================
out("=== maps ===");
m = {"a": 1, "b": 2};
out(m);               # {"a": 1, "b": 2}
out(m["a"]);          # 1
out(m.has("a"));      # true
out(m.has("z"));      # false
out(m.size());        # 2

m.set("c", 3);
out(m);               # {"a": 1, "b": 2, "c": 3}

out(m.keys());        # ["a", "b", "c"]
out(m.values());      # [1, 2, 3]

m.delete("a");
out(m);               # {"b": 2, "c": 3}

m["d"] = 4;
out(m["d"]);          # 4

# ============================================================
# 7. STRINGS
# ============================================================

test_string :: String = "    trim   ";

out("=== strings ===");
s = "Hello, World";
out(s.uppercase());         # HELLO, WORLD
out(s.lowercase());         # hello, world
out(s.find("World"));       # 7
out(s.substr(0, 5));        # Hello
out(s.substr(7));           # World
out(test_string.trim());     # trim
out(s.replace("World", "Vyne"));  # Hello, Vyne

# ============================================================
# 8. TERNARY AND NULL COALESCE
# ============================================================
out("=== ternary/coalesce ===");
out(5 > 3 ? "yes" : "no");  # yes

a = null;
out(a ?? 42);               # 42

b = 7;
out(b ?? 42);               # 7

# ??= assignment
c = null;
c ??= 100;
out(c);                     # 100
c ??= 200;
out(c);                     # 100 (unchanged)

# ============================================================
# 9. IN OPERATOR
# ============================================================
out("=== in operator ===");
list = [1, 2, 3];
out(2 in list);             # true
out(5 in list);             # false

mm = {"k": 1};
out("k" in mm);             # true
out("z" in mm);             # false

out("ell" in "hello");      # true

# ============================================================
# 10. PIPELINE
# ============================================================
out("=== pipeline ===");

fn double(n :: Int64) -> Int64 {
    return n * 2;
}

out(5 |> double);           # 10
out(5 |> double |> double); # 20
out(10 |> add(3));          # 13

# ============================================================
# 11. INTERPOLATED STRINGS
# ============================================================
out("=== interp ===");
name = "World";
out("Hello, {name}!");      # Hello, World!

greeting = "Vyne";
out("{greeting} says hi");  # Vyne says hi

# ============================================================
# 12. ENUMS
# ============================================================
out("=== enums ===");

enum Color {
    Red,
    Green = 10,
    Blue
};

out(Color.Red);             # 0
out(Color.Green);           # 10
out(Color.Blue);            # 11

# ============================================================
# 13. GROUPS (static namespaces)
# ============================================================
out("=== groups ===");

group Config {
    VERSION :: Int64 = 1;
    NAME = "vyne";
};

out(Config.VERSION);        # 1
out(Config.NAME);           # vyne

# ============================================================
# 14. STRUCTS / INTERFACES
# ============================================================
out("=== structs ===");

interface Point {
    x :: Float64,
    y :: Float64
}

p = Point(3.0, 4.0);
out(p.x);                   # 3.0
out(p.y);                   # 4.0

p.x = 10.0;
out(p.x);                   # 10.0

# ============================================================
# 15. BUILT-IN FUNCTIONS
# ============================================================
out("=== builtins ===");
out(sizeof(list));          # 3
out(type(42));              # Int64
out(type(3.14));            # Float64
out(type("x"));             # String
out(int64(3.99));           # 3
out(float64(7));            # 7.0
out(string(123));           # 123
out(sequence(1, 5));        # [1, 2, 3, 4, 5]

# ============================================================
# 16. FOR LOOP MODES
# ============================================================
out("=== loop modes ===");

nums = [1, 2, 3, 4, 5, 6];

evens = through n :: nums filter { n % 2 == 0 };
out(evens);                 # [2, 4, 6]

doubled = through n :: nums collect { n * 2 };
out(doubled);               # [2, 4, 6, 8, 10, 12]

allEven = through n :: nums every { n % 2 == 0 };
out(allEven);               # false

allPos = through n :: nums every { n > 0 };
out(allPos);                # true

uniq = through n :: [1, 1, 2, 2, 3, 3] unique { };
out(uniq);                  # [1, 2, 3]

# ============================================================
# 17. DEEP COPY SEMANTICS (array args)
# ============================================================
out("=== deep copy ===");

fn mutate(a :: Array) {
    a[0] = 999;
    return null;
}

data = [1, 2, 3];
out(data);                  # [1, 2, 3]
mutate(data);
out(data);                  # [1, 2, 3] (unchanged)

# ============================================================
# 18. NESTED STRUCTS AND MAPS
# ============================================================
out("=== nested ===");
nested = {"inner": [1, 2, 3], "count": 5};
out(nested["inner"]);       # [1, 2, 3]
out(nested["count"]);       # 5

# ============================================================
# 19. STRING CONCAT WITH +
# ============================================================
out("=== concat ===");
out("a" + "b");             # ab
out("x" + 1);               # x1
out(1 + "y");               # 1y

# ============================================================
# 20. ARRAY CONCAT
# ============================================================
out("=== array concat ===");
out([1, 2] + [3, 4]);       # [1, 2, 3, 4]

# ============================================================
# DONE
# ============================================================
out("done");