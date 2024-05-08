=============
Constexpr cat
=============

.. contents::
	:depth: 2
	:local:


Constexpr cat implements a way to concatenate strings during compile time.
Is is extremely specific and strange, and should not be used under standard circumstances.

.. note:: It is currently only used by Json library.

.. note:: For concatenation at runtime use `base::strConcat`.

Code doc 
========

.. doxygenfile:: constexpr_cat.hpp
	:sections: briefdescription detaileddescription

.. doxygenfunction:: base::cat

.. doxygendefine:: CONSTEXPR_CAT

Usage
=====


.. code-block:: cpp
	:caption: Example

	#include <base/constexpr_cat.hpp>
	#include <iostream>
	
	int main() {
		constexpr a = CONSTEXPR_CAT("A", "B", "C");
		std::cout << a;
	}
