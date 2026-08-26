fun example_one_stmt(x: i32) -> i64 = x + 1;

fun example(x: i32) -> i64 = {
    return x - 1;
}

fun tuples(x: i32) -> (i32, i64) = {
    var tmp = (2*x, x+1);

    return tmp;
}

fun big_example(xyz: i64) -> i64 = {
    # @TODO: #1498 uncomment this test
    # let x: i8  = 0i8;
    
    let y: i16 = 1i16; 
    let z: i32 = 2;
    
    # if (xyz > 0) {
    #     return x;
    # }
    if (xyz > 10) {
        return y;
    }
    if (xyz > 20) {
        return z;
    }
    if (xyz > 30) {
        # @TODO: #1498 change to x+y+z
        return y + z;
    }
}
