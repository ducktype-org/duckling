fun pickAType(n: i64, t: type) -> type = {
    if (n == 1) {
        return bool;
    } else {
        return ref t;
    }
}

fun main() -> i64 = {
    var a: i64 = 42;
    var strange_variable: pickAType(2, i64) = &a;
    return strange_variable;
}