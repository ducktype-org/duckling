# Test ternary expressions and chained else-if
# Focus: "if cond then x else y" syntax and else-if chains

fun printer_returner(x: i64) -> i64 = {
    builtin_output_i64(67);
    return x;
}

fun main() -> i64 = {
    # Test ternary with literal condition
    let ternary1 = if true then 100 else 200;
    builtin_output_i64(ternary1);      # 100

    let ternary2 = if false then 300 else 400;
    builtin_output_i64(ternary2);      # 400

    # Test ternary with variable condition
    let x = 5;
    let ternary3 = if x > 3 then 50 else 60;
    builtin_output_i64(ternary3);      # 50

    let ternary4 = if x < 3 then 70 else 80;
    builtin_output_i64(ternary4);      # 80

    # Test ternary with arithmetic in results
    let a = 10;
    let ternary5 = if a > 5 then a * 2 else a / 2;
    builtin_output_i64(ternary5);      # 20

    # Test nested ternary (requires parentheses per docs)
    let cond = true;
    let ternary6 = if cond then 1 else (if false then 2 else 3);
    builtin_output_i64(ternary6);      # 1

    let ternary7 = if false then 10 else (if true then 20 else 30);
    builtin_output_i64(ternary7);      # 20

    # Test ternary result in expression
    let y = 7;
    let computed = (if y > 5 then 100 else 50) + 25;
    builtin_output_i64(computed);      # 125

    # Test chained else-if
    var val: i64 = 1;
    if (val == 1) {
        builtin_output_i64(11);        # 11
    } else if (val == 2) {
        builtin_output_i64(12);
    } else {
        builtin_output_i64(13);
    }

    val = 2;
    if (val == 1) {
        builtin_output_i64(21);
    } else if (val == 2) {
        builtin_output_i64(22);        # 22
    } else {
        builtin_output_i64(23);
    }

    val = 3;
    if (val == 1) {
        builtin_output_i64(31);
    } else if (val == 2) {
        builtin_output_i64(32);
    } else {
        builtin_output_i64(33);        # 33
    }

    # Test multiple else-if branches
    val = 4;
    if (val == 1) {
        builtin_output_i64(41);
    } else if (val == 2) {
        builtin_output_i64(42);
    } else if (val == 3) {
        builtin_output_i64(43);
    } else if (val == 4) {
        builtin_output_i64(44);        # 44
    } else {
        builtin_output_i64(45);
    }

    # Test ternary lazy evaluation
    builtin_output_i64(if true then printer_returner(46) else printer_returner(48));      # 67\n46\n
    builtin_output_i64(if false then printer_returner(49) else printer_returner(50));     # 67\n50\n
    # Only one printer action should happen, the one from active branch

    return 0;
}
