#{
# For cases when code lir emits a=b+c instructions.
#}
fun assignmentChainedWithArithmetics() -> () = {
    var a: i64 = 0;
    var b: i64 = 1;
    
    a = b + a;
    builtin_output_i64(a); # 1

    a = 0;
    b = 1;
    a = a + b;
    builtin_output_i64(a); # 1

    a = 10;
    b = 20;
    a = a + b;
    builtin_output_i64(a); # 30

    a = 10;
    b = 20;
    a = a * b;
    builtin_output_i64(a); # 200

    a = 1;
    b = 2;

    var c: i64 = 0;
    c = a + b;
    builtin_output_i64(c); # 3

    c = a * b;
    builtin_output_i64(c); # 2
}

fun aliasingArithmetic() -> () = {
    var a = 10i64;
    var x = 3i64;
    x = a - x;
    builtin_output_i64(a);
    builtin_output_i64(x);

    let temp = 4i64;
    var y: ref i64 = &temp;

    y = -a;
    builtin_output_i64(a);
    builtin_output_i64(y);
}

fun main() -> i64 = {
    assignmentChainedWithArithmetics();
    aliasingArithmetic();
    return 0;
}
