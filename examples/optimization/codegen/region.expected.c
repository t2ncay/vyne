#include "vyne/runtime/vyne_runtime.h"
#include "vyne/runtime/modules/vmem.h"
int main(void) {
    // --- region: probe (bump) ---
    VyneValue vmem_cp_0 = vmem_runtime_checkpoint();
    {
        double v_values[3];
        int64_t si_1 = 0;
        if (VYNE_UNLIKELY(si_1 < 0 || si_1 >= 3)) {
            fprintf(stderr, "Runtime error (VNE-072): scratch index out of bounds: dim 0 of shape [3] got %lld at line 4\n", (long long)si_1);
            exit(1);
        }
        v_values[si_1] = 1.0;
        int64_t si_2 = 1;
        if (VYNE_UNLIKELY(si_2 < 0 || si_2 >= 3)) {
            fprintf(stderr, "Runtime error (VNE-072): scratch index out of bounds: dim 0 of shape [3] got %lld at line 5\n", (long long)si_2);
            exit(1);
        }
        v_values[si_2] = 2.0;
        int64_t si_3 = 2;
        if (VYNE_UNLIKELY(si_3 < 0 || si_3 >= 3)) {
            fprintf(stderr, "Runtime error (VNE-072): scratch index out of bounds: dim 0 of shape [3] got %lld at line 6\n", (long long)si_3);
            exit(1);
        }
        v_values[si_3] = 3.0;
        int64_t si_4 = 0;
        if (VYNE_UNLIKELY(si_4 < 0 || si_4 >= 3)) {
            fprintf(stderr, "Runtime error (VNE-072): scratch index out of bounds: dim 0 of shape [3] got %lld at line 7\n", (long long)si_4);
            exit(1);
        }
        double idx_5 = v_values[si_4];
        int64_t si_6 = 1;
        if (VYNE_UNLIKELY(si_6 < 0 || si_6 >= 3)) {
            fprintf(stderr, "Runtime error (VNE-072): scratch index out of bounds: dim 0 of shape [3] got %lld at line 7\n", (long long)si_6);
            exit(1);
        }
        double idx_7 = v_values[si_6];
        double bin_8 = idx_5 + idx_7;
        int64_t si_9 = 2;
        if (VYNE_UNLIKELY(si_9 < 0 || si_9 >= 3)) {
            fprintf(stderr, "Runtime error (VNE-072): scratch index out of bounds: dim 0 of shape [3] got %lld at line 7\n", (long long)si_9);
            exit(1);
        }
        double idx_10 = v_values[si_9];
        double bin_11 = bin_8 + idx_10;
        double v_total = bin_11;
        if ((v_total != 6.0)) {
            {
                exit((int)vyne_int(1).as.i64);
            }
        }
        VyneValue str_12 = vyne_to_string(vyne_float(v_total));
        VyneValue bin_13 = vyne_binop(vyne_string_static("V03 region: "), str_12, 29);
        vyne_out(bin_13);
    }
    vmem_runtime_rewind(vmem_cp_0);
    arena_free_all();
    return 0;
}
