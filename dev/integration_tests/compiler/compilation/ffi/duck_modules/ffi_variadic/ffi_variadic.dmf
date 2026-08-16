import core.builtins.*;

extern("C") {
    # In this file SSE register is floating-point register.
    # Small (8 bytes)  -> x86-64: single INTEGER reg, coerced to `{ i64 }`.
    class Small { x: i32 = 0; y: i32 = 0; }

    namespace variadic {
        @cffi_variadic_fixed_params(1) fundecl sum_varargs_mixed(count: i64, a: i64, b: f64) -> i64;

        @cffi_variadic_fixed_params(1) fundecl sum_varargs_struct(count: i64, a: Small, b: Small) -> i64;
    }
}

fun main() -> i64 = {
    # Variadic calls
    # One INTEGER and one SSE variadic argument.
    builtin_output_i64(variadic::sum_varargs_mixed(1i64, 4i64, 2.5));          # 6

    # Structs passed by value through `...`.
    builtin_output_i64(variadic::sum_varargs_struct(2i64, Small(1, 2), Small(3, 4)));  # 10

    return 0i64;
}
