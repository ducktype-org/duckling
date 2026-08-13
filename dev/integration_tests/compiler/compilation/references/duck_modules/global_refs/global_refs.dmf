var glob: i64 = 100;
var blob: i64 = 200;

fun foo(a: ref i64, val: i64) -> () = {
    a = val;
    builtin_output_i64(blob);
}

fun goo(a: ref i64) -> () = {
    a = 888;
}

fun ret_ref(a: ref i64) -> ref i64 = {
    a = 999;
    return a; # This will not get auto derefed.
}

fun ret_val(a: ref i64) -> i64 = {
    a = 888;
    return a; # This will get auto derefed.
}


fun main() -> i64 = {
    var r_blob = &blob;
    foo(&blob, 999);
    builtin_output_i64(r_blob); # 999
    foo(r_blob, 888);
    builtin_output_i64(r_blob); # 888

    let a: i64 = ret_ref(&blob);
    builtin_output_i64(blob); # 999
    let b: i64 = ret_val(&blob);
    builtin_output_i64(blob); # 888

    return 0;
}
