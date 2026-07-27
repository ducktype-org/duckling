import core.builtins.*;

class SimpleClass {
    var x: i64;
}

template(T: type, Q: type)
class Pair {
    var t: T;
    var q: Q;
}

fun main() -> i64 = {
    var p: Pair:{i64, SimpleClass};
    p.t = 5;
    p.q = SimpleClass(x = 10);

    builtin_output_i64(p.t);
    builtin_output_i64(p.q.x);

    # note: here the template argument is a unit value (not a unit type),
    # but it correctly coerces to the unit type, when used in the Pair class
    var p2: Pair:{(), i64};
    p2.t = ();
    p2.q = 42;

    builtin_output_i64(p2.q);

    return 0;
}
