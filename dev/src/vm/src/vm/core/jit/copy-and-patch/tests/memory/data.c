int foo(int x) {
	if (x <= 1)
		return 1;
	else
		return foo(x - 1) + foo(x - 2);
}

int goo(int x) {
	int output = 0;
	for (int i = 0; i < x; ++i) output += foo(i) * foo(i);
	return output;
}
