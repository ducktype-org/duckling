import core.runtime;

# Simple self-contained test for units as values and types.

let a: () = ();
let b: unitType = ();
const unitType: type = ();

fun foo(u: ()) -> () = {
    runtime.dvm.builtin_output_i64(1i64);
    return u;
}

fun main() -> i64 = {
    foo(a);
    foo(b);
    return 0i64;
}
