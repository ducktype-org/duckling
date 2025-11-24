#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/types/ints.hpp>

#include <iostream>

MAKE_STRINGIFYABLE_ENUM(n, u16, MyEnum, A, B, C)

// NOLINTBEGIN
int main() {
	n::MyEnum enum_value = n::MyEnum::A;

	std::cout << base::enumToStr(enum_value) << "\n";  // "A"
	enum_value = base::strToEnum<n::MyEnum>("B");
}

// NOLINTEND
