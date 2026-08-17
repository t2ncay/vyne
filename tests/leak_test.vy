ruleset { warnings, dynamic_casting };
module vmem;

# ==============================================================================
# MEMORY LEAK & VMEM STRESS TEST SCRIPT FOR VYNE INTERPRETER
# ==============================================================================

out("=== STARTING VYNE ENGINE VMEM STRESS TEST ===");

initial_ram :: Float64 = vmem.usage();
out("Initial Interpreter RAM Usage (vmem.usage): " + string(initial_ram) + " bytes");

i :: Int64 = 0;
max_iterations :: Int64 = 200000;

through i :: 0..max_iterations -> loop {
    
    # 1. Test transient string allocations & concatenations
    temp_str :: String = "CYBERWARFARE_NODE_" + string(i) + "_PACKET_DATA_STREAM";
    
    # 2. Test temporary array allocations
    temp_arr :: Array = [i, i + 1, i + 2, temp_str];
    
    # 3. Test temporary map allocations
    temp_map :: Map = {
        "id": i,
        "status": "active",
        "payload": temp_str
    };

    # Track RAM usage via vmem every 50,000 steps
    if (i % 50000 == 0) {
        current_ram :: Float64 = vmem.usage();
        out("Iteration: " + string(i) + " | vmem.usage(): " + string(current_ram) + " bytes");
    }
};

final_ram :: Float64 = vmem.usage();
out("=== STRESS TEST COMPLETED ===");
out("Final Interpreter RAM Usage (vmem.usage): " + string(final_ram) + " bytes");
out("Difference: " + string(final_ram - initial_ram) + " bytes");