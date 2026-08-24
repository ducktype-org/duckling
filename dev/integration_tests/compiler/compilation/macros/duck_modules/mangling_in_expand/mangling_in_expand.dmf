namespace N {
    expand "fun foo() = 1";
}

namespace M {
    expand "fun foo() = 2";
}

fun main() -> i64 = {
    builtin_output_i64(N.foo());
    builtin_output_i64(M.foo());

    return 0;
}
