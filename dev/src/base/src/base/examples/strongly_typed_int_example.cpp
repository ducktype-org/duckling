#include <base/strongly_typed_int.hpp>

#include <iostream>

STRONG_TYPEDEF_INT(MyInt, int);
STRONG_TYPEDEF_INT_DIMENSIONAL(Kg, int);

int main() {
	MyInt value = MyInt(0);  // ok
	value += MyInt(2);       // ok
	value *= MyInt(2);       // ok

	Kg weight = Kg(0);
	weight += Kg(2);  // ok
	weight *= 2;      // ok
	// weight *= weight; // error

	int raw_value = int(weight);  // ok, explicit
	raw_value     = int(value);   // ok, explicit
	std::cout << raw_value << std::endl;
}
