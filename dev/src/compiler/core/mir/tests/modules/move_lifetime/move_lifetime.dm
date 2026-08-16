fun eat(x: i64) = x;

# No move: the local is alive at its scope end, so it gets an unconditional `Destruct`.
fun noMove() = {
    var a: i64 = 5;
    eat(a);
}

# Move on only one branch: at the merge the local is `MaybeMoved`, so it gets a
# conditional `DestructIf`.
fun maybeMove(c: i64) = {
    var a: i64 = 5;
    if (c == 0) {
        eat(move a);
    }
}

# Unconditional move in the entry block followed by another block. Used by the liveness-map
# unit test to observe a `Moved` reaching state in successor blocks.
fun moveParamThenBlock(a: i64, c: i64) = {
    eat(move a);
    if (c == 0) {
        eat(1);
    }
}
