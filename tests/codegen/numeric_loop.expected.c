// --- Globals ---
VyneValue fn_sum_squares(int arg_count, VyneValue* args);
// --- Functions ---
// fn: sum_squares
VyneValue fn_sum_squares(int arg_count, VyneValue* args) {
    int64_t v_sum_squares_n = ((arg_count > 0)) ? ((args[0].type == V_INT64) ? args[0].as.i64 : (int64_t)args[0].as.f64) : 0;
    VyneValue __ret_sum_squares = vyne_null();
    int __returning_sum_squares = 0;
    int64_t v_sum_squares_total = 0;
    int64_t lo_i_0 = 1;
    int64_t hi_i_1 = v_sum_squares_n;
    for (int64_t i_2 = lo_i_0; i_2 <= hi_i_1; ++i_2) {
        int64_t v_i = i_2;
        {
            int64_t bin_3 = v_i * v_i;
            int64_t bin_4 = v_sum_squares_total + bin_3;
            v_sum_squares_total = bin_4;
        }
    }
    return vyne_int(v_sum_squares_total);
    return __ret_sum_squares;
}
int main(void) {
    VyneValue* args_6 = (VyneValue*)arena_alloc(sizeof(VyneValue) * 1);
    args_6[0] = vyne_int(10);
    VyneValue ret_5 = fn_sum_squares(1, args_6);
    vyne_out(ret_5);
    arena_free_all();
    return 0;
}
