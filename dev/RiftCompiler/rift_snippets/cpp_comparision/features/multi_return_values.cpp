#include <tuple>

std::tuple<int, int> divRem1(int a, int b) {
	return {a / b, a % b};
}

struct DivRem {
	int div;
	int rem;
};

DivRem divRem2(int a, int b) {
	return {a / b, a % b};
}

// Commented out since it is not-idiomatic in Duckling, and
// rarely used in modern C++:
// void divRem3(int a, int b, int& div, int& rem) {
// 	div = a / b;
// 	rem = a % b;
// }

// int main() {
// 	auto [div, rem] = divRem1(5, 2);
// 	auto [div2, rem2] = divRem2(5, 2);
// 	int div3, rem3;
// 	divRem3(5, 2, div3, rem3);
// }
