
var a: i64 = 1;

namespace N {
    expand "fun foo() -> i64 = 1 + 1;";
}

expand "fun bar() -> i64 = a;";

fun main() -> i64 = {
    a = 2;
    builtin_output_i64(N.foo());
    builtin_output_i64(bar());
    return 0;
}
