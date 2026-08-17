fun testBigRealloc() -> () = {
    var l: List[i64];
    var i = 0;
    while (i < 100) {
        l.push(i);
        i = i + 1;
    }

    builtin_output_u64(l.length()); # 100

    builtin_output_i64(l[0]);   # 0
    builtin_output_i64(l[49]);  # 49
    builtin_output_i64(l[99]);  # 99
}

fun testPushPopPushPopPushPop() -> () = {
    var l: List[i64];
    
    var i = 0;
    while (i < 10) { l.push(i); i = i + 1; }
    
    l.pop(5u64);
    builtin_output_u64(l.length()); # 5
    
    i = 0;
    while (i < 20) { l.push(i + 100); i = i + 1; }
    
    builtin_output_u64(l.length()); # 25
    builtin_output_i64(l[0]);       # 0
    builtin_output_i64(l[4]);       # 4
    builtin_output_i64(l[5]);       # 100
    builtin_output_i64(l[24]);      # 119
}

fun testPopTooBig() -> () = {
    var l: List[i64];
    l.push(1); l.push(2);
    
    l.pop(10u64); # Try removing 10 elements, although only 2 exist.
    builtin_output_u64(l.length()); # 0
    
    l.push(555);
    builtin_output_i64(l[0]); # 555
}

fun main() -> i32 = {
    testBigRealloc();
    testPushPopPushPopPushPop();
    testPopTooBig();
    
    return 0;
}
