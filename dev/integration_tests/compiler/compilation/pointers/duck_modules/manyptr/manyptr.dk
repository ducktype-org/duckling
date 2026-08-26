extern("C") fundecl malloc(siae: i64) -> cptr i8;
extern("C") fundecl free(p: cptr i8);


class SomeClass {
    x: i8[8];
    y: i64;

    fun print() = {
        builtin_output_i64(x[0]);
        builtin_output_i64(y);
    }
}

fun testInts() = {
    # Alloc array of 3 elements, each 4 bytes
    var a = malloc(3 * 4) as manyptr i32;
    a[0] = 1;
    a[1] = 2 + a[0];
    a[2] = 3 + a[1] + a[0];

    builtin_output_i64(a[0]);
    builtin_output_i64(a[1]);
    builtin_output_i64(a[2]);
    free(a as cptr i8);
}

fun testClasses() = {
    # Alloc of 3 elements, each 16 bytes
    var a = malloc(3 * 16) as manyptr SomeClass;
    a[0].x[0] = 42i8;
    a[0].y = 43;
    a[0].print();
    free(a as cptr i8);
}

fun testArrays() = {
    # Alloc of 3 elements, each 16 bytes (2 elements array of pointers)
    var ar = malloc(3 * 16) as manyptr (manyptr i32)[2];
    ar[0][0] = malloc(2 * 4) as manyptr i32;
    ar[0][0][0] = 30;
    builtin_output_i64(ar[0][0][0]);
    free(ar[0][0] as cptr i8);
    free(ar as cptr i8);
}


fun main() -> i64 = {
    testInts();
    testClasses();
    testArrays();

    return 0;
}
