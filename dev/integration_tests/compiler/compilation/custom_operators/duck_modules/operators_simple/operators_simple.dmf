fun +*(a: i64, b: i64) -> i64 = {
    return a * b + a;
}

fun -*(a: i64) -> i64 = {
    return a * 6;
}

# @TODO: #3131 Add case for suffix standalone operator

class Foo {
    x: i64 = 0;

    fun +/(a: i64) -> i64 = {
        return x * a + x;
    }

    fun -/() -> i64 = {
        return x * 6;
    }

    # @TODO: #3131 Add case for suffix method operator
}

fun main() -> i64 = {
    let x = 7 +* 5;
    builtin_output_i64(x);
    let y = -* 7;
    builtin_output_i64(y);

    # @TODO: #3133 Have the methods have the same names as the standalone operators.
    var fu = Foo(7);
    let u = fu +/ 5;
    builtin_output_i64(u);
    var fv = Foo(7);
    let v = -/fv;
    builtin_output_i64(v);

    return 0;
}
