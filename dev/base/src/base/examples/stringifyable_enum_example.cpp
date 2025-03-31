#include <iostream>

#include <base/ints.hpp>
#include <base/string_id.hpp>
#include <base/stringifyable_enum.hpp>

MAKE_STRINGIFYABLE_ENUM(N, u16, MyEnum, A, B, C)

// NOLINTBEGIN
int main() {
	N::MyEnum enum_value = N::MyEnum::A;

	std::cout << base::enumToStr(enum_value).strView() << "\n";  // "A"
	enum_value = base::strToEnum<N::MyEnum>(base::StrID("B"));
}

// NOLINTEND
