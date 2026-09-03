# Test boolean expressions in various contexts
# Focus: ternary if-expressions, comparison results, complex boolean logic

fun isEven(n: i64) -> bool = n % 2 == 0;
fun isPositive(n: i64) -> bool = n > 0;

fun main() -> i64 = {
    # Test ternary if expression (per docs: if cond then expr1 else expr2)
    let result1 = if true then 100 else 200;
    builtin_output_i64(result1);        # 100

    let result2 = if false then 100 else 200;
    builtin_output_i64(result2);        # 200

    # Test ternary with variable condition
    let x = 5;
    let result3 = if x > 3 then 50 else 60;
    builtin_output_i64(result3);        # 50

    # Test ternary with comparison expression
    let a = 10;
    let b = 20;
    let max = if a > b then a else b;
    builtin_output_i64(max);            # 20

    # Test boolean comparison results stored in variables
    let isLess = a < b;                 # true
    let isEqual = a == b;               # false
    if (isLess) {
        builtin_output_i64(1);          # 1
    }
    if (not isEqual) {
        builtin_output_i64(2);          # 2
    }

    # Test boolean function results
    if (isEven(4)) {
        builtin_output_i64(3);          # 3
    }
    if (isPositive(10) and isEven(10)) {
        builtin_output_i64(4);          # 4
    }

    # Test complex boolean expressions
    let complex = (a > 0) and (b > 0) or (a == b);
    if (complex) {
        builtin_output_i64(5);          # 5
    }

    # Test nested ternary (per docs: requires parentheses)
    let nested = if true then 10 else (if false then 20 else 30);
    builtin_output_i64(nested);         # 10

    # Test ternary in expressions
    let computed = (if a < b then a else b) + 5;
    builtin_output_i64(computed);       # 15

    return 0;
}
