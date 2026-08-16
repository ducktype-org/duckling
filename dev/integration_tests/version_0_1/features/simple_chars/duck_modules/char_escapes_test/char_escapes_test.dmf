# Test char escape sequences
# Focus: escape characters per docs: \n, \t, \r, \\, \', \"

fun main() -> i64 = {
    # Test newline escape
    var newline: char = '\n';
    builtin_output_i64(newline as i64); # 10 (ASCII for newline)

    # Test tab escape
    var tab: char = '\t';
    builtin_output_i64(tab as i64);     # 9 (ASCII for tab)

    # Test carriage return escape
    var carriage: char = '\r';
    builtin_output_i64(carriage as i64);# 13 (ASCII for carriage return)

    # Test backslash escape
    var backslash: char = '\\';
    builtin_output_i64(backslash as i64); # 92 (ASCII for backslash)

    # Test single quote escape
    var singleQuote: char = '\'';
    builtin_output_i64(singleQuote as i64); # 39 (ASCII for single quote)

    # Test double quote escape (in char)
    var doubleQuote: char = '\"';
    builtin_output_i64(doubleQuote as i64); # 34 (ASCII for double quote)

    # Test escape sequence comparisons
    if (tab < newline) {
        builtin_output_i64(1);        # 1 (9 < 10)
    }

    if (carriage > newline) {
        builtin_output_i64(2);        # 2 (13 > 10)
    }

    # Test escape chars in array-like usage
    var escapes_sum: i64 = newline as i64 + tab as i64 + carriage as i64;
    builtin_output_i64(escapes_sum);  # 32 (10 + 9 + 13)

    # Test escape equality
    var newline2: char = '\n';
    if (newline == newline2) {
        builtin_output_i64(3);        # 3
    }

    # Verify all escape values are distinct
    if (newline != tab and tab != carriage and carriage != backslash) {
        builtin_output_i64(4);        # 4
    }

    return 0;
}
