var c: i64 = 20 + g;
var g: i64 = hoo();

fun foo() = {
	c = g;
	var a: i64 = 23;
	while (9) {
		var b: i64 = 7;
	}
	if (9) {
		var b: i64 = 7;
	}
	return 0;
}

fun goo() = {
	var a: i32 = 23;
	a = 24;
	c = hoo();
}

fun hoo() -> i64 = {
	c = c + 1;
	g = c + hoo();
	return 33;
}
