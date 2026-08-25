var copies: i64 = 0;

class Counter {
    id: i32 = 0;
    Counter.copy(other: const ref Counter) = {
        copies = copies + 1;
        return Counter(other.id);
    }
}

class TwoCounters { 
    a: Counter;
    b: Counter;
}

class Mixed {
    n: i32 = 0;
    c: Counter;
    ref_c: ref Counter;
    ptr_c: ptr Counter;
}

fun main() -> i64 = {
    var t: (i32, Counter) = (1, Counter(2));
    var t2: (i32, Counter) = copy t;    # +1
    builtin_output_i64(copies);         # 1

    var two: TwoCounters = TwoCounters(Counter(1), Counter(2));
    var two2: TwoCounters = copy two;   # +2
    builtin_output_i64(copies);         # 3

    var mx: Mixed = Mixed(7, Counter(1), &two.a, &two.a as ptr Counter);
    var mx2: Mixed = copy mx;           # +1
    builtin_output_i64(copies);         # 4

    var arr: Counter[3];
    var arr2: Counter[3] = copy arr;    # +3
    builtin_output_i64(copies);         # 7

    return 0;
}
