import submodule as sm;

fun inc(x: i64) -> i64 = {
    return x + 1;
}

fun add(x: i64, y: i64) -> i64 = {
    return x + y;
}

fun main() -> i64 = {
    let number_1: i64 = builtin_input_i64();
    let number_2: i64 = builtin_input_i64();

    builtin_output_i64(number_1 + number_2 * 2);

    builtin_output_i64(add(1, inc(2)));

    sm.inc_2(1);

    return 0;
}
