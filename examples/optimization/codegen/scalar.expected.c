#include "vyne/runtime/vyne_runtime.h"
// --- Globals ---
int64_t v_answer = 0;
int main(void) {
    int64_t bin_0 = 6 * 7;
    v_answer = bin_0;
    if ((v_answer != 42)) {
        {
            exit((int)vyne_int(1).as.i64);
        }
    }
    VyneValue str_1 = vyne_to_string(vyne_int(v_answer));
    VyneValue bin_2 = vyne_binop(vyne_string_static("V03 scalar: "), str_1, 29);
    vyne_out(bin_2);
    arena_free_all();
    return 0;
}
