==================
Strongly typed int
==================

.. contents::
	:depth: 2
	:local:

Provides macros for creating a strongly typed integer. They are created as classes that generally behave the same as other integral types but can only be explicitly cast.

Functionalities
===============

Two variants are provided:

.. doxygendefine:: STRONG_TYPEDEF_INT_DIMENSIONAL

.. doxygendefine:: STRONG_TYPEDEF_INT

Usage
=====


.. code-block:: cpp
	:caption: Example

	#include <base/strongly_typed_int.hpp>

	STRONG_TYPEDEF_INT(MyInt, int);
	STRONG_TYPEDEF_INT_DIMENSIONAL(Kg, int);

	int main() {
		MyInt value = MyInt(0); //ok
		value += MyInt(2); //ok
		value *= MyInt(2); //ok

		Kg weight = Kg(0);
		weight += Kg(2); // ok
		weight *= 2; // ok
		// weight *= weight; // error

		int raw_value = int(weight); // ok, explicit
		raw_value = int(value); // ok, explicit
	}
