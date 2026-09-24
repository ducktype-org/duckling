fun numeric() -> i64 = {
    let c = 3;

    #    (           true           )
    #   3 >= 3 < 4 >= 3 == 3 <= 3 >= 3
    if (3 >= c < 4 >= c == c <= c >= c) {}
    else {
        return 1;
    }

    #   (false) true  true
    #   -2 <= -3 == -3 != 3
    if (-2 <= -c == -c != c) { # first comparison is important
        return 2;

        #       true  (false) true
        #      -4 <= -3 != -3 != 3
    } else if (-4 <= -c != -c != c) { # middle comparison is important
        return 3;

        #       true  true (false)
        #      -4 <= -3 == -3 != 3
    } else if (-4 <= -c == -c == c) { # last comparison is important
        return 4;
    }

    #           (false) true true
    #             5 < 3 <= 4 > 9
    let d: bool = 5 < c <= 4 > 9; # first
    if (d) {
        return 5;
    }

    #               (      true      )                   true true true true
    #                3 == 3 != 3 == 3                    2 < 3 <= 3 <= 3 < 7
    let e: bool = if c == c != c == c  then false   else 2 < c <= 3 <= c < 7;
    if (e) {
    } else {
        return 6;
    }

    return 0;
}

var counter = 0;

fun inc(x: i64) -> i64 = {
    counter = counter + 1;
    return x;
}

fun single_evaluation() -> i64 = {

    let inc_chain = inc(1) < inc(2) < inc(3);

    if (counter != 3) {
        return counter;
    }

    return 0;
}

fun main() -> i64 = {
    return numeric() + 10 * single_evaluation();
}
