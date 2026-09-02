# Variable shadowing and class-based overloading test
# This test checks variable shadowing and operator overloading for classes

class Counter {
    value: i64 = 0;
}

# Overload + operator for Counter
fun +(a: Counter, b: Counter) -> Counter = {
    return Counter(a.value + b.value);
}

# Overload == operator for Counter
fun ==(a: Counter, b: Counter) -> bool = {
    return a.value == b.value;
}

fun main() -> i64 = {
    # 1. Variable shadowing with primitives
    var x: i64 = 5;
    {
        var x: i64 = 10; # Shadows outer x
        if (x == 10) builtin_output_i64(1); # Should execute
    }
    if (x == 5) builtin_output_i64(2); # Should execute

    # 2. Nested shadowing
    var y: i64 = 1;
    {
        var y: i64 = 2;
        {
            var y: i64 = 3;
            if (y == 3) builtin_output_i64(3); # Should execute
        }
        if (y == 2) builtin_output_i64(4); # Should execute
    }
    if (y == 1) builtin_output_i64(5); # Should execute

    # 3. Variable shadowing with class fields
    var counter = Counter(7);
    {
        var value: i64 = 42; # Shadows counter.value
        if (value == 42 and counter.value == 7) builtin_output_i64(6); # Should execute
    }
    if (counter.value == 7) builtin_output_i64(7); # Should execute

    # 4. Shadowing method parameter (simulate by local scope)
    var param: i64 = 100;
    {
        var param: i64 = 200;
        if (param == 200) builtin_output_i64(8); # Should execute
    }
    if (param == 100) builtin_output_i64(9); # Should execute

    # 5. Negative check: no shadowing
    var z: i64 = 50;
    if (z == 50) builtin_output_i64(10); # Should execute

    # 6. Operator overloading for classes
    var c1 = Counter(3);
    var c2 = Counter(4);
    var c3 = c1 + c2;
    if (c3.value == 7) builtin_output_i64(11); # Should execute
    if (c1 == c2) builtin_output_i64(12); # Should not execute

    return 0;
}
