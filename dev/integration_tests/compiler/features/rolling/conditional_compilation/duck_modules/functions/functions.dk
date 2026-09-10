@backend_dependent
fundecl printValue();

@dvm_only_impl
fun printValue() = {
    builtin_output_i64(10);
}

@native_only_impl
fun printValue() = {
    builtin_output_i64(20);
}

@backend_dependent
fundecl getValue(p: i32) -> i32;

@dvm_only_impl
fun getValue(p: i32) = {
    return 10 + p;
}

@native_only_impl
fun getValue(p: i32) = {
    return 20 + p;
}

fun main() -> i64 = {
    # Comptime always sees the dvm version of the function
    const v = getValue(1);
    builtin_output_i64(v);

    # But the backend compiled runtime code sees it's 
    # backend version of the function
    printValue();
    return 0;
}
