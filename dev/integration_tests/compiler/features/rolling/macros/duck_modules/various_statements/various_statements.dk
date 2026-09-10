expand " import submodule as s; ";

expand " var a: i64 = 1; ";

expand " namespace N { fun foo() -> i64 = { return 42; } } ";

expand " class T { var x: i64 = 0; var y: i64 = 0; } ";

expand " const c = 100; ";

fun main() -> i64 = {
    var t = T();
    t.x = 10;
    t.y = 20;

    a = 4;

    var sum = t.x + t.y + N.foo() + a + c + s.sub();

    builtin_output_i64(sum);

    return 0;
}
