__attribute__((visibility("hidden"), always_inline)) inline static int foo(int x) {
	if (x <= 1)
        return 1;
	else
        return foo(x - 1) + foo(x - 2);
    }

extern "C" __attribute__((visibility("protected"))) 
int foo_export(int x) {
    return foo(x);
}

__attribute__((visibility("hidden"))) inline static int goo(int x) {
    int output = 0;
	for (int i = 0; i < x; ++i) output += foo(i) * foo(i);
    return output;
}

extern "C" __attribute__((visibility("protected"))) 
int goo_export(int x) {
    return goo(x);
}
