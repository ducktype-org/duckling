#include <iostream>
#include <stdfloat>

int main() {
#if defined(__STDCPP_FLOAT16_T__)
	std::cout << "Float found!\n";
#else
	std::cout << "Float not found!\n";
#endif

	return 0;
}
