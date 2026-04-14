ruleset { warnings, dynamic_casting };
#===============================================================================
# Vyne Language Comprehensive Test Suite
#===============================================================================
use lib "vcolors.vy";  # Test external module import

#===============================================================================
# 1. BASIC TYPES AND OPERATIONS
#===============================================================================

out("=== 1. BASIC TYPES AND OPERATIONS ===");

# Integer operations
a :: Int64 = 42;
b :: Int64 = 10;
out("a = " + string(a) + ", b = " + string(b));
out("a + b = " + string(a + b));
out("a - b = " + string(a - b));
out("a * b = " + string(a * b));
out("a / b = " + string(a / b));
out("a % b = " + string(a % b));

# Float operations
x :: Float64 = 3.14;
y :: Float64 = 2.0;
out("x = " + string(x) + ", y = " + string(y));
out("x + y = " + string(x + y));
out("x * y = " + string(x * y));
out("x / y = " + string(x / y));

# String operations
str1 :: String = "Hello";
str2 :: String = " World";
out(str1 + str2);  # String concatenation
out("String length: " + string(str1.length()));

# Boolean operations
t = true;
f = false;
out("true && false = " + string(t && f));
out("true || false = " + string(t || f));
out("!true = " + string(!t));

#===============================================================================
# 2. COMPARISON OPERATORS
#===============================================================================

out("\n=== 2. COMPARISON OPERATORS ===");

out("5 > 3: " + string(5 > 3));
out("5 < 3: " + string(5 < 3));
out("5 >= 5: " + string(5 >= 5));
out("5 <= 4: " + string(5 <= 4));
out("5 == 5: " + string(5 == 5));
out("5 != 6: " + string(5 != 6));

#===============================================================================
# 3. ARRAY OPERATIONS
#===============================================================================

out("\n=== 3. ARRAY OPERATIONS ===");

# Array creation
empty_arr :: Array = [];
nums :: Array = [1, 2, 3, 4, 5];
out("nums = " + string(nums));
out("nums[0] = " + string(nums[0]));
out("nums[4] = " + string(nums[4]));

# Array methods
out("\n--- Array Methods ---");
out("nums.size() = " + string(nums.size()));

nums.push(6);
nums.push(7);
out("After push(6,7): " + string(nums));

last = nums.pop();
out("After pop(): " + string(nums) + " (popped: " + string(last) + ")");

out("nums.back() = " + string(nums.back()));

nums.sort();
out("After sort(): " + string(nums));

nums.reverse();
out("After reverse(): " + string(nums));

nums.place_all(0, 3);
out("After place_all(0,3): " + string(nums));

nums.clear();
out("After clear(): " + string(nums));

# 2D Arrays
matrix :: Array = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];
out("\n2D Matrix:");
out(matrix);
out("matrix[1][1] = " + string(matrix[1][1]));  # Should print 5

#===============================================================================
# 4. STRING INDEXING
#===============================================================================

out("\n=== 4. STRING INDEXING ===");

text :: String = "Vyne Language";
out("text = " + text);
out("text[0] = " + text[0]);      # 'V'
out("text[5] = " + text[5]);      # 'L'
out("text.length() = " + string(text.length()));

#===============================================================================
# 5. RANGES AND SEQUENCES
#===============================================================================

out("\n=== 5. RANGES AND SEQUENCES ===");

range1 = 1..5;
out("1..5 = " + string(range1));

range2 = 1.5..5.5;
out("1.5..5.5 = " + string(range2));

seq = sequence(0, 5);
out("sequence(0,5) = " + string(seq));

#===============================================================================
# 6. CONTROL FLOW (IF/ELSE)
#===============================================================================

out("\n=== 6. CONTROL FLOW ===");

test_val :: Int64 = 15;

if test_val > 10 {
    out(string(test_val) + " is greater than 10");
} else {
    out(string(test_val) + " is not greater than 10");
}

# If-else chain
score :: Int64 = 85;
if score >= 90 {
    out("Grade: A");
} else if score >= 80 {
    out("Grade: B");
} else if score >= 70 {
    out("Grade: C");
} else {
    out("Grade: F");
}

#===============================================================================
# 7. LOOPS (WHILE, THROUGH)
#===============================================================================

out("\n=== 7. LOOPS ===");

# While loop
out("While loop counting to 5:");
counter :: Int64 = 1;
while counter <= 5 {
    out("  " + string(counter));
    counter++;
}

# Through loop (standard iteration)
out("\nThrough loop (collect):");
collected = through item :: [10, 20, 30, 40, 50] -> collect {
    item * 2
};
out("Doubled: " + string(collected));

# Through loop (filter)
out("\nThrough loop (filter):");
evens = through item :: 1..10 -> filter {
    item % 2 == 0
};
out("Even numbers: " + string(evens));

# Through loop (unique)
out("\nThrough loop (unique):");
duplicates = [1, 2, 2, 3, 3, 3, 4];
uniques = through item :: duplicates -> unique {
    item
};
out("Unique values: " + string(uniques));

#===============================================================================
# 8. FUNCTIONS
#===============================================================================

out("\n=== 8. FUNCTIONS ===");

# Simple function
fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

out("add(5, 3) = " + string(add(5, 3)));

# Function with no return
fn greet(name :: String) {
    out("Hello, " + name + "!");
}
greet("Vyne");

# Function with type inference
fn multiply(x, y) {
    return x * y;
}
out("multiply(4, 5) = " + string(multiply(4, 5)));
out("multiply(2.5, 3.0) = " + string(multiply(2.5, 3.0)));

# Recursive function
fn factorial(n :: Int64) -> Int64 {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}
out("factorial(5) = " + string(factorial(5)));

#===============================================================================
# 9. INTERFACES AND STRUCTS
#===============================================================================

out("\n=== 9. INTERFACES AND STRUCTS ===");

# Define an interface
interface Person {
    name :: String,
    age :: Int64,
    email :: String
}

# Create instances
john :: Person = Person("John Doe", 30, "john@example.com");
jane :: Person = Person("Jane Smith", 25, "jane@example.com");

out("John: " + string(john));
out("Jane: " + string(jane));

# Access fields
out("John's name: " + john.name);
out("Jane's age: " + string(jane.age));

# Modify fields
john.age = 31;
out("John's new age: " + string(john.age));

# List fields
out("Person fields: " + string(john.fields()));

# Nested interfaces
interface Address {
    street :: String,
    city :: String,
    zip :: Int64
}

interface Contact {
    person :: Person,
    address :: Address
}

home :: Address = Address("123 Main St", "Springfield", 12345);
contact :: Contact = Contact(john, home);
out("\nContact: " + string(contact));

#===============================================================================
# 10. MODULES AND NAMESPACES
#===============================================================================

out("\n=== 10. MODULES AND NAMESPACES ===");

# Define a module
module MathUtils;

# Functions in the module
fn :: MathUtils square(x :: Int64) -> Int64 {
    return x * x;
}

fn :: MathUtils cube(x :: Int64) -> Int64 {
    return x * x * x;
}

# Use module functions
out("MathUtils.square(7) = " + string(MathUtils.square(7)));
out("MathUtils.cube(3) = " + string(MathUtils.cube(3)));

# Groups within modules

#===============================================================================
# 11. POSTFIX OPERATIONS (INCREMENT/DECREMENT)
#===============================================================================

out("\n=== 11. POSTFIX OPERATIONS ===");

count :: Int64 = 5;
out("Initial count: " + string(count));
out("count++ = " + string(count++));
out("After increment: " + string(count));
out("count-- = " + string(count--));
out("After decrement: " + string(count));

#===============================================================================
# 12. UNARY OPERATIONS
#===============================================================================

out("\n=== 12. UNARY OPERATIONS ===");

flag = true;
out("!flag = " + string(!flag));

neg_num :: Int64 = 42;
out("-neg_num = " + string(-neg_num));

# Address operator (if supported)
# ptr = $count;
# out("Address of count: " + string(ptr));

#===============================================================================
# 13. BUILT-IN FUNCTIONS
#===============================================================================

out("\n=== 13. BUILT-IN FUNCTIONS ===");

val = 42;
out("type(val) = " + type(val));
out("string(val) = " + string(val));
out("int64(3.14) = " + string(int64(3.14)));
out("float64(10) = " + string(float64(10)));
out("sizeof(val) = " + string(sizeof(val)));

#===============================================================================
# 14. TYPE CONVERSIONS
#===============================================================================

out("\n=== 14. TYPE CONVERSIONS ===");

int_val :: Int64 = 100;
float_val :: Float64 = float64(int_val);
out("int_val = " + string(int_val) + " (type: " + type(int_val) + ")");
out("float_val = " + string(float_val) + " (type: " + type(float_val) + ")");

str_num :: String = "123";
parsed_int = int64(str_num);
out("Parsed string to int: " + string(parsed_int));

#===============================================================================
# 15. ERROR HANDLING TESTS (commented out to avoid crashes)
#===============================================================================

out("\n=== 15. ERROR HANDLING (VALID CASES) ===");

# Create a fresh array for testing
test_array :: Array = [10, 20, 30, 40, 50];
out("test_array = " + string(test_array));
out("Valid array access test_array[0] = " + string(test_array[0]));
out("Valid array access test_array[2] = " + string(test_array[2]));

# These would cause errors - uncomment to test error handling
# out(test_array[100]);  # Index out of bounds
# out(undefined_var);    # Undefined variable
# out("string"[10]);      # String index out of bounds

#===============================================================================
# 16. COMPLEX EXAMPLES
#===============================================================================

out("\n=== 16. COMPLEX EXAMPLES ===");

# Fibonacci sequence
fn fibonacci(n :: Int64) -> Array {
    result = [];
    a = 0;
    b = 1;
    
    through i :: 0..n-1 -> loop {
        if i == 0 {
            result.push(a);
        } else if i == 1 {
            result.push(b);
        } else {
            next = a + b;
            result.push(next);
            a = b;
            b = next;
        }
    };
    
    return result;
}

out("Fibonacci(10): " + string(fibonacci(10)));

# Matrix operations (requires vlinalg module)
# Uncomment when vlinalg is available
# m1 :: Matrix = Matrix(2, 2, [[1, 2], [3, 4]]);
# m2 :: Matrix = Matrix(2, 2, [[5, 6], [7, 8]]);
# result = vlinalg.add(m1, m2);
# out("Matrix addition result: " + string(result));

#===============================================================================
# 17. CONSTANTS AND READ-ONLY VALUES
#===============================================================================

out("\n=== 17. CONSTANTS ===");

const PI :: Float64 = 3.14159;
out("PI = " + string(PI));
# PI = 3.14;  # This would cause an error - constant reassignment

#===============================================================================
# 19. DEPLOY AND MODULE MANAGEMENT
#===============================================================================

out("\n=== 19. MODULE MANAGEMENT ===");

# Deploy a module (makes it available)
deploy MathUtils;

# Check if module exists (if you have a way to check)
out("MathUtils module deployed");

#===============================================================================
# 20. PERFORMANCE TEST (optional)
#===============================================================================

out("\n=== 20. PERFORMANCE TEST ===");

# Simple loop performance test
start = 0;
end = 1000;
sum = 0;

through i :: start..end -> loop {
    sum = sum + i;
};

out("Sum of 0..1000 = " + string(sum));

out(vcolors.green("\n=== ALL TESTS COMPLETED SUCCESSFULLY ==="));