# Valid and invalid `move` usages, exercised by mir_lifetime_test::moveValidationTest.
# `good*` functions must lower successfully, `bad*` functions must fail with a
# use-after-move / use-of-possibly-moved error.

fun good1(c: i64) = {
    var a: i64 = 10;
    var b: i64 = 15;
    a = move b;
    a = 3;
}

fun good2(c: i64) = {
    var a: i64 = 10;
    var b: i64 = 15;
    if (b < 16){
        a = move b;
    }
    a = 3;
}

fun good3(c: i64) = {
    var a: i64 = 10;
    while (a < 16){
        var b: i64 = 15;
        if (b < 16){
            b = 4;
            a = move b;
        }
    }
    a = 3;
}

fun good4(c: i64) = {
    var a: i64 = 15;
    while (a < 16){
        var b: i64 = 10;
        a = move b;
    }
    a = 3;
}

# `b` is moved, then read again on the same straight-line path.
fun bad1(c: i64) = {
    var a: i64 = 10;
    var b: i64 = 15;
    a = move b;
    a = b;
}

# `b` is moved inside the loop body, so the loop condition reads a moved value on the
# next iteration.
fun bad2(c: i64) = {
    var a: i64 = 10;
    var b: i64 = 15;
    while (b < 16){
        a = move b;
    }
}

# `b` is moved on only one branch, so it is possibly-moved at the read below.
fun bad3(c: i64) = {
    var a: i64 = 10;
    var b: i64 = 15;
    if (b < 16){
        a = move b;
    }
    a = b;
}

fun eat(x: i64) = x;

# `reinit*` functions exercise reinitialization by assignment: a local is moved out and then
# reassigned (a bare-local store, which carries a `Reinit` flag), so a later read is valid.

# Straight-line: `b` moved out, then reinitialized, then read.
fun reinit1() = {
    var a: i64 = 10;
    var b: i64 = 15;
    a = move b;
    b = 99;
    eat(b);
}

# `a` is moved and reinitialized on both branches, so it is alive at the merge read.
fun reinit2() = {
    var a: i64 = 10;
    var c: i64 = 15;
    if (c == 0) { eat(move a); a = 5; }
    else { eat(move a); a = 6; }
    eat(a);
}

# `a` is moved then reinitialized inside the loop body, so both the in-body read and the
# next-iteration / post-loop reads see an alive value.
fun reinit3() = {
    var a: i64 = 10;
    var c: i64 = 15;
    while (c > 0) {
        eat(move a);
        a = 2;
        eat(a);
        c = c - 1;
    }
    eat(a);
}
