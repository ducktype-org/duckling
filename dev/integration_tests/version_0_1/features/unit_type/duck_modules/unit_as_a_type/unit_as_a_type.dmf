const UnitType: type = ();
const globalUnit: UnitType = ();

fun unit_identity(u: ()) -> () = u;

fun consume_unit(u: ()) -> i64 = 42;

fun side_effect_proc(u: ()) -> () = {
    builtin_output_i64(4);
    return u;
}

fun unit_overload(x: ()) -> i64 = 7;
fun unit_overload(x: i64) -> i64 = 9;

fun make_unit() -> () = {
    return ();
}

fun main() -> i64 = {
    var a: UnitType = ();
    let b: () = globalUnit;
    let c: () = unit_identity(a);

    builtin_output_i64(1);

    if (consume_unit(b) == 42) builtin_output_i64(2);
    if (unit_overload(c) == 7) builtin_output_i64(3);

    side_effect_proc(unit_identity(()));

    if (unit_overload(11) == 9) builtin_output_i64(5);

    let d: UnitType = make_unit();
    if (consume_unit(d) == 42) builtin_output_i64(6);

    return 0;
}