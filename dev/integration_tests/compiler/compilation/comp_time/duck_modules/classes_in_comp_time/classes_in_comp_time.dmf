
class T {
    a: i64;
}

class Q {
    var t: T;
    var b: i64 = 0;
}

fun useTQ() -> i64 = {
    var t: T;
    t.a = 5;
    var q1: Q;
    var q2 = Q(t = t, b = 1);
    var q3 = Q(t = t);

    return t.a + q1.b + q2.b + q3.b + q3.t.a; # 5+0+1+0+5 = 11
}


const a = useTQ();


fun main() -> i64 = {
    builtin_output_i64(a);
    return 0;
}
