var copies: i64 = 0;

class Counter {
    id: i32 = 0;
    Counter.copy(other: const ref Counter) = {
        copies = copies + 1;
        return Counter(other.id);
    }
}

fun byRef(r: ref Counter) -> i32 = r.id;
fun byVal(c: Counter) -> i32 = c.id;
fun makeCounter() -> Counter = Counter(9);

fun main() -> i64 = {
    var a: Counter = Counter(1);

    var x: i32 = byRef(&a);             # no copy
    builtin_output_i64(copies);         # 0

    var y: i32 = byVal(copy a);         # +1
    builtin_output_i64(copies);         # 1

    var z: i32 = byVal(makeCounter());  # temporary argument - moved, no copy
    builtin_output_i64(copies);         # 1

    return 0;
}
