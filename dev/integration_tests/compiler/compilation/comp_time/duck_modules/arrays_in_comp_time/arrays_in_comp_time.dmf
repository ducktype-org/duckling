class T { a: i64; }

fun useArray() -> i64 = {
    var arr: T[3];
    arr[0] = T(1);
    arr[1] = T(2);
    arr[2] = T(3);

    return arr[0].a + arr[1].a + arr[2].a; # 1 + 2 + 3 = 6
}

const a = useArray();

fun main() -> i64 = {
    builtin_output_i64(a);
    return 0;
}
