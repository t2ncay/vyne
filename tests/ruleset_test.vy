# ============================================================================
# ALL RULESETS DECLARED AT THE TOP
# ============================================================================

ruleset {
    warnings = all,
    warnings_ignore = [unused_variable, implicit_type],
    type_check = hybrid,
    implicit_casting = warn,
    profiling = on,
    debug = on,
    trace = on
};

# ============================================================================
# MODULE DECLARATION
# ============================================================================

module ruleset_test;

# ============================================================================
# TEST FUNCTIONS
# ============================================================================

fn test_unused_warning() {
    x = 10;
    y = 20;
    out("Testing warnings");
    z = 30;
}

fn test_shadow_warning() {
    value = 5;
    if (true) {
        value = 10;
        out(value);
    }
}

data = [];

fn test_memory_tracking() {
    through i :: 1..100 -> loop {
        data.push(i);
    };
    out("Data size: ", data.size());
}

fn test_all_warnings() {
    a = 10;
    b = 20;
    out(a + b);
    
    if (true) {
        out("True branch");
        return;
        out("Unreachable");
    }
}

fn test_error_only_warnings() {
    x = 100;
    y = 200;
    out("x + y = ", x + y);
    
    i = 0;
    while (i < 10) {
        out(i);
        if (i > 5) {
            break;
        }
        i = i + 1;
    }
}

fn test_no_warnings() {
    unused1 = 1;
    unused2 = 2;
    unused3 = 3;
    out("No warnings here!");
}

fn test_with_warnings_and_dynamic() {
    z = 5;
    out("z = ", z);
    
    z = "hello";
    out("z = ", z);
}

fn test_no_warnings_here() {
    a = 10;
    b = 20;
    out(a + b);
}

fn test_complex() {
    value = "Starting";
    out("Value: ", value);
    value = 100;
    out("Value: ", value);
    value = 99.99;
    out("Value: ", value);
    
    numbers = [5, 3, 8, 1, 9, 2, 7, 4, 6];
    numbers.sort();
    out("Sorted: ", numbers);
    
    numbers.push(10);
    out("After push: ", numbers);
    
    popped = numbers.pop();
    out("Popped: ", popped);
    
    john = Person("John Doe", 30);
    out(john.greet());
    john.birthday();
    out("John's age: ", john.age);
    
    scores = {"math": 95, "science": 88, "history": 92};
    out("Scores: ", scores);
    out("Math score: ", scores["math"]);
    
    evens = through i :: 1..20 -> filter {
        return i % 2 == 0;
    };
    out("Evens: ", evens);
    
    squares = through i :: 1..10 -> collect {
        return i * i;
    };
    out("Squares: ", squares);
    
    if (5 in squares) {
        out("5 is in squares");
    }
    
    big_list = [];
    through i :: 1..100 -> collect {
        big_list.push("Item " + i);
    };
    out("Big list size: ", big_list.size());
}

fn test_perf() {
    result = "";
    through i :: 1..100 -> loop {
        result = result + "x";
    };
    out("Result length: ", result.size());
}

# ============================================================================
# INTERFACE (STRUCT) DEFINITION
# ============================================================================

interface Person {
    name :: String,
    age :: Int64,
    
    greet() -> String {
        return "Hello, " + self.name;
    }
    
    birthday() {
        self.age = self.age + 1;
    }
}

# ============================================================================
# ENTRY POINT
# ============================================================================

fn main() {
    out("========================================");
    out("=== VYNE RULESET TEST SUITE ===");
    out("========================================");
    out("");
    
    test_unused_warning();
    test_shadow_warning();
    test_memory_tracking();
    test_all_warnings();
    test_error_only_warnings();
    test_no_warnings();
    test_with_warnings_and_dynamic();
    test_no_warnings_here();
    test_complex();
    test_perf();
    
    out("");
    out("========================================");
    out("=== ALL TESTS COMPLETED ===");
    out("========================================");
}

# Run the tests
main();