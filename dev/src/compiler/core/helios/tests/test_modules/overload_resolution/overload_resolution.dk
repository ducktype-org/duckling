# Test overload resolution with different parameter types and scenarios


class MyClass {
    value: i64;
}

fun foo(x: bool) -> i64 = {
    return 1;
}

fun foo(x: f64) -> i64 = {
    return 2;
}

fun foo(x: MyClass) -> i64 = {
    return 3;
}

fun foo(x: i64) -> i64 = {
    return 3;
}

var CALL_FOO_BOOL: i64 = foo(get_bool());
var CALL_FOO_FLOAT: i64 = foo(get_f64());
var CALL_FOO_CLASS: i64 = foo(MyClass(42));
var CALL_FOO_I64: i64 = foo(get_i64());

fun goo(x: i64) -> i64 = {
    return 10;
}

fun goo(y: i64) -> i64 = {
    return 20;
}

# Named parameter disambiguates which function to call
var CALL_GOO_X: i64 = goo(x = 30);
var CALL_GOO_Y: i64 = goo(y = 40);


fun goo(x: f64, y: bool) -> i64 = {
    return 42;
}

fun goo(x: i64, y: bool) -> i64 = {
    return 42;
}

# Float is not implicitely convertible to int, so the f64 is closer than i64
var CALL_GOO_F64 = goo(get_f32(), true);


# Helpers
# Using values directly would mean there is a coercion from const T -> T
# which is equal to other coercions like i64 -> f64 and would be ambiguous coercion matches.

fun get_f64() -> f64 = {
    return 3.;
}

fun get_f32() -> f32 = {
    return 3.;
}

fun get_i64() -> i64 = {
    return 3;
}

fun get_bool() -> bool = {
    return true;
}
