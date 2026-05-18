fun main() -> i64 = {
    # Test boolean literals
    var t: bool = true;
    var f: bool = false;

    # Test boolean variable assignment
    if (t) {
        builtin_output_i64(1); # Should execute
    }
    if (f) {
        builtin_output_i64(2); # Should not execute
    }

    # Test boolean negation
    if (not f) {
        builtin_output_i64(3); # Should execute
    }
    if (not t) {
        builtin_output_i64(4); # Should not execute
    }

    # Test boolean and
    if (t and t) {
        builtin_output_i64(5); # Should execute
    }
    if (t and f) {
        builtin_output_i64(6); # Should not execute
    }

    # Test boolean or
    if (t or f) {
        builtin_output_i64(7); # Should execute
    }
    if (f or f) {
        builtin_output_i64(8); # Should not execute
    }

    # Test boolean xor
    if (t xor f) {
        builtin_output_i64(9); # Should execute
    }
    if (t xor t) {
        builtin_output_i64(10); # Should not execute
    }

    # Test reassignment
    t = false;
    f = true;
    if (f and not t) {
        builtin_output_i64(11); # Should execute
    }

    return 0;
}
