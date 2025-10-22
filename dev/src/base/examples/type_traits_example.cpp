#include <base/comptime/type_traits.hpp>

#include <iostream>

template<class T>
struct Q {
	// ...
};

template<class T>
struct QQ: public Q<T> {
	// ...
};

template<class T>
struct G {
	// ...
};

int main() {
	std::cout << base::IsInstantiationOf<int, Q> << "\n";              // false
	std::cout << base::IsInstantiationOf<Q<int>, Q> << "\n";           // true

	std::cout << base::IsNumber<int> << "\n";                          // true
	std::cout << base::IsNumber<float> << "\n";                        // true
	std::cout << base::IsNumber<std::string> << "\n";                  // false

	std::cout << base::IsOfSameClass<Q<int>, Q<int>> << "\n";          // true
	std::cout << base::IsOfSameClass<Q<int>, Q<std::string>> << "\n";  // true
	// std::cout << base::IsOfSameClass<Q<int>, int> << "\n"; // error
	std::cout << base::IsOfSameClass<Q<int>, G<int>> << "\n";   // false
	std::cout << base::IsOfSameClass<Q<int>, QQ<int>> << "\n";  // false

	std::cout << base::typeName<int>() << "\n";                 // "int"
	std::cout << base::typeName<Q<int>>() << "\n";              // "Q<int>"
}
