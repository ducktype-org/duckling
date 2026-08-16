fun dynamic_array_test() -> () = {
    var l: List[i64];
    l.push(1);
    let len_l = l.length();
    l.pop(1u64);
    l[0] = 42;
}
