#include <base/misc/anycast.hpp>

#include <iostream>

int main() {
	std::any x = std::string{ "123" };

	std::cout << base::anyCast<std::string>(x);
	// 123

	std::cout << base::anyCast<int>(x);
	/*
	    terminate called after throwing an instance of 'base::LogicError'
	    what():  Bad any_cast: Value is of different type than "int"
	*/
}
