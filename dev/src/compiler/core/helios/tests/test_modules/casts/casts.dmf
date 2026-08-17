fun test_casts() -> i64 = {
	# explicit cast from i64 literal -> f64
	var explicit: f64 = 1 as f64;

	# implicit widening: i32 -> i64
	var widen: i64 = get_i32();

	# bool to integer coercion (implicit)
	var bool_as_int: i32 = true;

	# integer to bool coercion (implicit)
	var int_as_bool: bool = 1;

	return 0;
}


fun get_i32() -> i32 = {
    return 42 as i32;
}
