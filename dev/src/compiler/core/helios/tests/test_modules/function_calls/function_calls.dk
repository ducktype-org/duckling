fun square(a: i64) -> i64 = {
    return a * a;
}

fun foo(a: i64, b: i64) -> i64 = {
    let a_squared: i64 = square(a);
 
    var V1: i64 = 1;
    let bb: i64 = 1 + V1;

    return square(a_squared + b);
}

fun foo1(a: i64, b:i64 = 5) -> i64 ={
    return 0;
}

fun main() ->i64 = {
    foo1(a = 3);
    foo1(8);
    foo1(8, 3);
    foo1(b = 4, a = 3);
    return 0;
}
