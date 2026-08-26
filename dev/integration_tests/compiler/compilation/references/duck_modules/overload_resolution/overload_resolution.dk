fun foo(a: ref i64) -> () = {
    builtin_output_i64(1);
}
fun foo(a: i64) -> () = {
    builtin_output_i64(2);
}


fun main() -> i64 = {
    let a: i64 = 123;
    let ref_a: ref i64 = &a;

    # @TODO: #1823 Reconsider that.
    foo(a);

    return 0;
}
