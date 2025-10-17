#include <base/extend_cpp/defer.hpp>

int main() {
	int a = 3;
	// a = 3
	{
		// a = 3
		a += 2;
		// a = 5
		defer(a++);
		// a = 5
		a--;
		// a = 4
	}
	// a = 5
	{
		defer(a = 4);
		defer(a--);
		// a = 5
	}
	// a = 4;
	{
		defer(a--);
		defer(a = 3);
		// a = 4
	}
	// a = 2
}
