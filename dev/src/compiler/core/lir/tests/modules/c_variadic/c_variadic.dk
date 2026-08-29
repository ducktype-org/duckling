extern("C") {
    @cffi_variadic_fixed_params(1) fundecl sum_varargs(count: i64, a: i64, b: f64) -> i64;

    # Invalid declarations.

    # C requires at least one named parameter before the `...`.
    @cffi_variadic_fixed_params(0) fundecl variadicZeroFixed(a: i64) -> i64;

    # Nothing past the split, so no variadic argument is described.
    @cffi_variadic_fixed_params(1) fundecl variadicNoVarArgs(a: i64) -> i64;

    # Variadic parameters must be "f64" or "i32/u32/i64/u64", the rest is invalid.
    @cffi_variadic_fixed_params(1) fundecl variadicUnpromotedFloat(a: i64, b: f32) -> i64;
    @cffi_variadic_fixed_params(1) fundecl variadicUnpromotedInt(a: i64, b: u16) -> i64;
}

fun caller() -> i64 = {
    return sum_varargs(2i64, 5i64, 1.5);
}
