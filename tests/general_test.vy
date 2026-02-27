# 1. Module Setup
module physics;
module vcore;
# 2. Interface Definition (Contract for later structs)
interface IPoint {
    x :: Float64;
    y :: Float64;
}

# 3. Group Definition with Extension (::)
# This tests: group [Name] :: [TargetModule]
group Constants :: physics {
    pi = 3.14159;
    gravity = 9.81;
    e = 2.718;
};

# 4. Nested Group within the same module
group Math :: physics {
    radius = 10.0;
    counter = 0;
};

# 5. Function Injection with Return Types
# Tests: func [Module] :: [Name] (params) -> [Type]
fn :: physics getArea(r :: Float64) -> Float64 {
    # Testing access to sibling group 'Constants'
    return physics.Constants.pi * (r * r);
}

# 6. Postfix and Range Expression Test
fn :: physics runCounter() {
    out("Testing Range and Postfix:");
    
    # Testing through (range) -> step mode
    through 1..3 -> loop {
        out("Current Step:");
        out(_);
        physics.Math.counter++; # Testing assignment to group member
    }
}

# 7. Control Flow and Conditionals
fn :: physics checkGravity() {
    if (physics.Constants.gravity > 5.0) {
        out("High Gravity Environment");
    } else {
        out("Low Gravity Environment");
    }
}

# --- Execution Block ---
out("--- Vyne Full Capability Test ---");

# Test 1: Static Member Access (Contextual)
# Since we are in 'physics' context, this finds physics.Constants.pi
out("Constants.pi:");
out(physics.Constants.pi);

# Test 2: Nested Path Resolution
# Explicitly calling through the module path
out("physics.Math.radius:");
out(physics.Math.radius);

# Test 3: Function Injected into Module
area = physics.getArea(physics.Math.radius);
out("Calculated Area (PI * 10^2):");
out(area);

# Test 4: State Mutation in Groups
physics.runCounter();
out("Math.counter value after loop:");
out(physics.Math.counter);

# Test 5: Logic and Conditionals
physics.checkGravity();

out("--- Test Successfully Completed ---");