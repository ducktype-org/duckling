#include <stdlib.h>

int simple_function_plus_1(int x) {
	return x + 1;
}

int calling_simple_odd(int x) {
	return simple_function_plus_1(2 * x);
}

int recursive_fibonacci(int x) {
	if (x <= 1)
		return 1;
	else
		return recursive_fibonacci(x - 1) + recursive_fibonacci(x - 2);
}

int calling_fibonacci_sum(int x) {
	int output = 0;
	for (int i = 0; i <= x; ++i) output += recursive_fibonacci(x) * recursive_fibonacci(x);
	return output;
}

int* calling_libc(int x) {
	int* output = calloc(sizeof(int), x);
	for (int i = 0; i < x; ++i) {
		output[i] = x;
	}
	return output;
}


