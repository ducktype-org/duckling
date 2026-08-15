fun arg1(a: i64) -> i64 = 0;
fun arg0() -> i64 = 0;

fun foo() -> i64 = {
    let x: i64 = 123;

    let y: i64 = arg1(x) + 20;
    let z: i64 = arg1(arg0());

    arg0();
    arg1(1);

    return arg1(z);
}

# The builtins implemented in LIR. They are declared here instead of imported from the standard
# library, so this module still needs nothing but the `@builtin` attribute.

template(T: type)
@builtin("move_out")
fundecl move_out(pointer: ptr T) -> T;

template(T: type)
@builtin("move_in")
fundecl move_in(pointer: ptr T, value: T) -> ();

# `value` is stored into the storage under `pointer`, without a call and without destroying what
# was there before.
fun writeInto(pointer: ptr i64, value: i64) -> () = {
    move_in:{i64}(pointer, value);
}

# The value under `pointer` is read out, without a call and without touching the storage.
fun readOut(pointer: ptr i64) -> i64 = {
    return move_out:{i64}(pointer);
}
