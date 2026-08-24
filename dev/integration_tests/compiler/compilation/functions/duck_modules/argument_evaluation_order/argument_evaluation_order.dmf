fun printAndReturn(value: i64) -> i64 = {
    builtin_output_i64(value);
    return value;
}

fun add(a: i64, b: i64) -> i64 = a + b;

fun main() -> i64 = {
    # The arguments must be evaluated left to right, so 1 is printed before 2.
    let sum = add(printAndReturn(1), printAndReturn(2));
    builtin_output_i64(sum);
    return 0;
}
