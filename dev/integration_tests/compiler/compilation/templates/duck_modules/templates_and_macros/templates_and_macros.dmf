import core.builtins.*;

expand "template(T: type) class X { var t: T; }";

template(s: str)
namespace Expand {
    expand s;
}


fun main() -> i64 = {
    var x: X:{i64};
    x.t = 42;
    builtin_output_i64(x.t);

    Expand:{"fun foo() = builtin_output_i64(42);"}.foo();

    return 0;
}
