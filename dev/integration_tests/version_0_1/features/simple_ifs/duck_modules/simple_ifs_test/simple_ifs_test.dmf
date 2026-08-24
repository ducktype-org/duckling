fun main() -> i64 = {
    # Test simple if
    if (true) {
        builtin_output_i64(1);       # 1
    }

    # Test if with false condition
    if (false) {
        builtin_output_i64(2);       # Should not execute
    }

    # Test if-else
    if (true) {
        builtin_output_i64(3);       # 3
    } else {
        builtin_output_i64(4);       # Should not execute
    }

    if (false) {
        builtin_output_i64(5);       # Should not execute
    } else {
        builtin_output_i64(6);       # 6
    }

    # Test if with variable condition
    var cond: bool = true;
    if (cond) {
        builtin_output_i64(7);       # 7
    }

    cond = false;
    if (cond) {
        builtin_output_i64(8);       # Should not execute
    } else {
        builtin_output_i64(9);       # 9
    }

    # Test if with comparison expressions
    var a: i64 = 10;
    var b: i64 = 5;
    if (a > b) {
        builtin_output_i64(10);      # 10
    }

    if (a < b) {
        builtin_output_i64(11);      # Should not execute
    } else {
        builtin_output_i64(12);      # 12
    }

    # Test nested if
    if (true) {
        if (true) {
            builtin_output_i64(13);  # 13
        }
    }

    # Test if with compound conditions
    if (a > 0 and b > 0) {
        builtin_output_i64(14);      # 14
    }

    if (a < 0 or b > 0) {
        builtin_output_i64(15);      # 15
    }

    # Test if-else with single statement (no braces)
    if (true)
        builtin_output_i64(16);      # 16

    return 0;
}
