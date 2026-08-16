import core.builtins.*;

var copies: i64 = 0;

class Counter {
    id: i32 = 0;
    Counter.copy(other: const ref Counter) = {
        copies = copies + 1;
        return Counter(other.id);
    }
}

class Wrapper {
    c: Counter;

    fun duplicate() -> Wrapper = {
        return copy self;
    }
}

fun copyFromRef(r: ref Counter) -> Counter = {
    return copy r;
}

fun main() -> i64 = {
    var c: Counter = Counter(7);

    # `copy` from reference gives a direct.
    var from_ref: Counter = copyFromRef(&c);    # +1
    builtin_output_i64(copies);                 # 1
    builtin_output_i64(from_ref.id as i64);     # 7

    # `copy` from `box` also gives a direct.
    var bc: box Counter = new Counter(9);
    var from_box: Counter = copy bc;            # +1
    builtin_output_i64(copies);                 # 2
    builtin_output_i64(from_box.id as i64);     # 9
     
    # `move` should not copy.
    var bc3: box Counter = move bc;             # Should not copy
    builtin_output_i64(copies);                 # 2

    var rebox: box Counter = new copy bc3;       # +1
    builtin_output_i64(copies);                 # 3
    builtin_output_i64(rebox.id as i64);        # 9

    var w: Wrapper = Wrapper(Counter(5));
    var w2: Wrapper = w.duplicate();            # +1
    builtin_output_i64(copies);                 # 4
    builtin_output_i64(w2.c.id as i64);         # 5

    # Check that it actually got copied.
    c.id = 100;
    builtin_output_i64(from_ref.id as i64);     # 7

    return 0;
}
