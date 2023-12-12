===========
Type traits
===========

Type traits provides set of functionalities useful during metaprogramming with types.

Functionalities
===============

* :code:`IsInstantiationOf<A, T>` -- Checks if a type :code:`A` is an instantiation of template :code:`T`.

Usage
-----

.. code-block:: cpp
	:caption: Example

	#include <base/type_traits.hpp>
	#include <iostream>

	template<class T>
	struct Q {...};

	int main() {
		std::cout << base::IsInstantiationOf<int, Q> << "\n"; // false
		std::cout << base::IsInstantiationOf<Q<int>, Q> << "\n"; // true
	}

