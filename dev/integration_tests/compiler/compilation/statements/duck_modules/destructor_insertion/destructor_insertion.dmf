fun A() -> i64 = {
    {
        var index: i64 = 0;
        let length: i64 = 3;

        while(index < length) {
            builtin_output_i64(index);
            index = index + 1;
        }
    };

    return 0;
}

fun B() -> i64 = {
    {
        var index: i64 = 0;
        let length: i64 = 4;

        while(index < length) {
            {
                var index_2: i64 = 0;
                let length_2: i64 = 2;

                while(index_2 < length_2) {
                    builtin_output_i64(index_2);
                    index_2 = index_2 + 1;
                    index = index + 1;
                }
            };
        }
    };

    return 0;
}

fun C() -> i64 = {
    var x: i64 = 1;
    if (x == 0) {
        var y: i64 = 1;
        x = x + y;
    } else {
        {
            var z: i64 = 2;
            {
                var w: i64 = 3;
                x = x + z + w;
            };
        };
    }
    return x;
}

fun main() -> i64 = {
    A();
    B();
    C();
    return 0;
}
