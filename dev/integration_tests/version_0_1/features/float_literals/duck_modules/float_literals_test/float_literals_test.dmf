fun main() -> i64 = {
    # 1) Scientific notation, positive and negative exponents, explicit +, no digit between . and e, etc.
    var sci1: f64 = 1.23e4;           # 12300.0
    if (sci1 == 12300.0) builtin_output_i64(1);

    var sci2: f64 = 5.67e-3;          # 0.00567
    if (sci2 == 0.00567) builtin_output_i64(2);

    var sci3: f64 = 1.0e+1;           # 10.0
    if (sci3 == 10.0) builtin_output_i64(3);

    var sci4: f64 = 1e2;              # 100.0
    if (sci4 == 100.0) builtin_output_i64(4);

    var sci5: f64 = 1.e3;             # 1000.0 (no digit after decimal)
    if (sci5 == 1000.0) builtin_output_i64(5);

    var sci6: f64 = .5e1;             # 5.0 (no digit before decimal)
    if (sci6 == 5.0) builtin_output_i64(6);

    var sci7: f64 = 0e0;              # 0.0 (zero exponent)
    if (sci7 == 0.0) builtin_output_i64(7);

    var sci8: f64 = 1.0e-0;           # 1.0 (negative zero exponent)
    if (sci8 == 1.0) builtin_output_i64(8);

    var sci9: f32 = 2.5e2f32;         # 250.0
    if (sci9 == 250.0f32) builtin_output_i64(9);

    var sci10: f32 = .25e+2f32;       # 25.0
    if (sci10 == 25.0f32) builtin_output_i64(10);

    var sci11: f32 = 3.e-1f32;        # 0.3
    if (sci11 == 0.3f32) builtin_output_i64(11);

    var sci12: f32 = 0e-5f32;         # 0.0
    if (sci12 == 0.0f32) builtin_output_i64(12);

    # Edge: very large and very small
    var sci13: f64 = 1e308;           # large, should not overflow
    if (sci13 == 1e308) builtin_output_i64(13);

    var sci14: f64 = 1e-308;          # small, should not underflow to zero
    if (sci14 == 1e-308) builtin_output_i64(14);

    # Edge: negative base
    var sci15: f64 = -2.5e3;          # -2500.0
    if (sci15 == -2500.0) builtin_output_i64(15);

    var sci16: f32 = -1.e+2f32;       # -100.0
    if (sci16 == -100.0f32) builtin_output_i64(16);

    return 0;
}
