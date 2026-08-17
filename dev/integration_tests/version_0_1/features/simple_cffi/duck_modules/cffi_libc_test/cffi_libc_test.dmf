# Test C FFI with a builtin function

extern("C") fundecl abs(x: i32) -> i32;

fun main() -> i64 = {
    # Test abs in expression context
    var diff: i32 = abs((5 - 10) as i32);
    builtin_output_i64(diff as i64);      # 5

    # Test abs in arithmetic expression
    var a: i32 = abs((-3) as i32) + abs((-4) as i32);
    builtin_output_i64(a as i64);         # 7

    # Test abs in comparison
    if (abs((-5) as i32) > abs((-3) as i32)) {
        builtin_output_i64(1);          # 1
    }

    # Test abs in loop
    var sum: i64 = 0;
    var i: i32 = (-3) as i32;
    while (i <= 0 as i32) {
        sum = sum + abs(i) as i64;
        i = i + 1 as i32;
    }
    builtin_output_i64(sum);            # 6 (3+2+1+0)

    # Test chained abs calls (redundant but tests nesting)
    var nested: i32 = abs(abs((-42) as i32));
    builtin_output_i64(nested as i64);    # 42

    # Test abs with function result
    var computed: i32 = abs(computeNegValue());
    builtin_output_i64(computed as i64);  # 100

    # Test abs in ternary-like pattern
    var val: i32 = (-7) as i32;
    var absResult = if val < 0 as i32 then abs(val) else val;
    builtin_output_i64(absResult as i64); # 7

    return 0;
}

fun computeNegValue() -> i32 = (-100) as i32;
