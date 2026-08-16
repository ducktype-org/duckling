# This test checks that a class containing a field which carries no information compiles successfully.

class UnitFieldClass {
    x: i32;
    unitField: ();
    y: i32;
}

fun main() -> i64 = {
    var c = UnitFieldClass(42, (), 69);
    builtin_output_i64(c.x);
    builtin_output_i64(c.y);
    return 0;
}
