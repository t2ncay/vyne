# distance_map.vy — compute the pairwise distance map for a protein structure.
#
# AlphaFold's Evoformer initializes its Pair Representation by computing the
# Euclidean distance between every pair of amino acid residues. 
# This script demonstrates how to do this efficiently in Vyne using 
# stack-allocated scratch arrays, bypassing the heap entirely.
#
# Pipeline:
#   vmath — generate mock 3D coordinates and compute square roots
#   vlin  — assemble the final Matrix
#   vmem  — region-based memory management

use external "vlin/vlin.vy";
use external "vcolors.vy";

use native vmath;
use native vmem;

ruleset { dynamic_casting };

vmath.seed(42);

# ======================================================================
# CONFIG
# ======================================================================
N_RESIDUES :: Int64 := 128;   # Size of the protein (N x N matrix)
DIM        :: Int64 := 3;     # 3D space (x, y, z)

# ======================================================================
# HELPERS
# ======================================================================

# Generate a mock protein structure of N residues.
# Returns a flattened array: [x1, y1, z1, x2, y2, z2, ...]
fn make_mock_protein(n :: Int64) -> Array<Float64> {
    coords :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        # Generate random coordinates in a 100 Angstrom box
        coords.push(vmath.random_float() * 100.0);
        coords.push(vmath.random_float() * 100.0);
        coords.push(vmath.random_float() * 100.0);
    };
    return coords;
}

# Compute the N x N pairwise distance map.
# Uses a bump region to allocate the scratch matrix on the C stack.
fn compute_distance_map(coords :: Array<Float64>, n :: Int64) -> Array<Float64> {
    
    # Allocate the distance matrix inside a bump region
    region distance_calc {
        # Allocate scratch memory on the C stack for the N x N matrix.
        # This avoids the heap entirely and is deeply cache-friendly.
        scratch dist_matrix :: Float64[n, n];
        
        # Calculate pairwise distances
        through i :: 0..n-1 -> loop {
            through j :: 0..n-1 -> loop {
                # Extract coordinates
                ix :: Int64 := i * 3;
                jx :: Int64 := j * 3;
                
                dx := coords[ix]   - coords[jx];
                dy := coords[ix+1] - coords[jx+1];
                dz := coords[ix+2] - coords[jx+2];
                
                # Euclidean distance: sqrt(dx^2 + dy^2 + dz^2)
                dist_matrix[i, j] = vmath.sqrt(dx*dx + dy*dy + dz*dz);
            };
        };
        
        # Flatten the 2D scratch array into a 1D array for vlin.Matrix
        dist_flat :: Array<Float64> := [];
        through i :: 0..n-1 -> loop {
            through j :: 0..n-1 -> loop {
                dist_flat.push(dist_matrix[i, j]);
            };
        };
        
        # Commit the result so it survives the region rewind
        region.commit(dist_flat);
    };
}

fn pad_left(s :: String, width :: Int64) -> String {
    n := int64(s.size());
    result := "";
    through i :: 0..width-n-1 -> loop { result = result + " "; };
    return result + s;
}

# ======================================================================
# HEADER
# ======================================================================
out("");
out(vcolors.bold("=== AlphaFold Distance Map Kernel ==="));
out("");
out("  residues       " + string(N_RESIDUES));
out("  matrix size    " + string(N_RESIDUES) + " x " + string(N_RESIDUES));
out("  total pairs    " + string(N_RESIDUES * N_RESIDUES));
out("");

# ======================================================================
# DATA
# ======================================================================
out("Generating mock protein coordinates...");
coords :: Array<Float64> := make_mock_protein(N_RESIDUES);

# ======================================================================
# COMPUTATION
# ======================================================================
out("Computing distance map...");

# vcore.now() returns a formatted "YYYY-MM-DD HH:MM:SS" String, not a
# numeric clock. Subtracting two such strings trips vyne_binop's
# invalid-operation branch. vcore.now_ns() is the monotonic Int64
# nanosecond clock that is actually meant for this.
start_ns := vcore.now_ns();

# Call the kernel. The scratch memory is freed instantly upon return.
dist_flat :: Array<Float64> := compute_distance_map(coords, N_RESIDUES);

# Assemble the final vlin Matrix
dist_map :: vlin.Types.Matrix := vlin.Types.Matrix(N_RESIDUES, N_RESIDUES, dist_flat);

end_ns := vcore.now_ns();
elapsed_ms := float64(end_ns - start_ns) / 1000000.0;

# ======================================================================
# FINAL
# ======================================================================
out("");
out(vcolors.bold("Done."));
out("  time elapsed   " + string(elapsed_ms) + " ms");
out("  checksum       " + string(dist_map.data[0] + dist_map.data[N_RESIDUES * N_RESIDUES - 1]));
out("");

# Optional: Print a tiny corner of the distance map to verify
out("  Distance map preview (top-left 3x3):");
through i :: 0..2 -> loop {
    line := "    [ ";
    through j :: 0..2 -> loop {
        val := dist_map.data[i * N_RESIDUES + j];
        # Format to 2 decimal places manually since we don't have printf
        scaled := int64(val * 100.0);
        ip := scaled / 100;
        fp := scaled % 100;
        line = line + string(ip) + "." + string(fp) + "  ";
    };
    line = line + "]";
    out(line);
};
out("");
