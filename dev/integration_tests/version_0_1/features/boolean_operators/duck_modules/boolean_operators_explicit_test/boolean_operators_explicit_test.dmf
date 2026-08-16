# Test boolean operators with explicit literals
# Per docs: and, or, not, xor operators

fun main() -> i64 = {
    # Testing AND operator (short-circuits if first operand is false)
    if (true and true) {
        builtin_output_i64(1); # Should execute
    }
    if (true and false) {
        builtin_output_i64(2); # Should not execute
    }
    if (false and true) {
        builtin_output_i64(99); # Should not execute, short-circuited
    }

    # Testing OR operator
    if (true or false) {
        builtin_output_i64(3); # Should execute
    }
    if (false or false) {
        builtin_output_i64(4); # Should not execute
    }
    if (true or true) {
        builtin_output_i64(5); # Should execute
    }
    if (false or true) {
        builtin_output_i64(6); # Should execute
    }

    # Testing NOT operator
    if (not false) {
        builtin_output_i64(7); # Should execute
    }
    if (not true) {
        builtin_output_i64(8); # Should not execute
    }

    # Testing XOR operator
    if (true xor false) {
        builtin_output_i64(9); # Should execute
    }
    if (true xor true) {
        builtin_output_i64(10); # Should not execute
    }
    if (false xor true) {
        builtin_output_i64(11); # Should execute
    }
    if (false xor false) {
        builtin_output_i64(12); # Should not execute
    }

    # Testing double negation
    if (not not true) {
        builtin_output_i64(13); # Should execute
    }
    if (not not false) {
        builtin_output_i64(14); # Should not execute
    }

    # Testing trifold boolean expressions
    if (true and true or false) {
        builtin_output_i64(15); # Should execute (precedence: and before or)
    }

    if (true and false or true) {
        builtin_output_i64(16); # Should execute (collapses to "false or true")
    }

    if (false or true or false) {
        builtin_output_i64(17); # Should execute
    }

    if (false or true and false) {
        builtin_output_i64(18); # Should not execute
    }

    if (false or true and true) {
        builtin_output_i64(19); # Should execute
    }

    return 0;
}
