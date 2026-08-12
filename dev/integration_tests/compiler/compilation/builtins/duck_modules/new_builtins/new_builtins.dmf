fun main() -> i64 = {
    var p = dvm_alloc_arr:{char}(8u64);
    p[0] = 'H';
    p[1] = 'E';
    p[2] = 'L';
    p[3] = 'L';
    p[4] = 'O';
    let a = p[0];

    dvm_realloc_arr:{char}(p, 16u64);
    p[5] = '\n';

    let s = slice_from_ptr_len:{char}(p, 6 as u64);
    builtin_output_str(s);

    dvm_free_arr:{char}(p);

    # Single-object allocation: `dvm_alloc` works on `ptr T` instead of `manyptr T`.
    var single = dvm_alloc:{char}();
    *single = a;
    builtin_output_char(*single);
    dvm_free:{char}(single);

    return 0i64;
}
