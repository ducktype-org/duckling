import core.runtime;

fun square(x : i64) -> i64 = {
    return x * x;
}

fun main() -> i64 = {
    let x: i64 = runtime.dvm.builtin_input_i64();
    let y: i64 = square(x);
    runtime.dvm.builtin_output_i64(y);
    runtime.dvm.builtin_output_i64(y + 1i64);
    return y + 1i64;
}
