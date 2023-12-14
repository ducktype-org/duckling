==================
Strongly typed int
==================

.. simple-description::

.. contents::
	:depth: 2
	:local:

Strongly typed int is a library that provides macros that creates a class implementing strongly typed integer.

Two variants are provided:

Functionalities
===============

:code:`STRONG_TYPEDEF_INT_DIMENSIONAL`
--------------------------------------

Creates dimensional integral type.


:code:`STRONG_TYPEDEF_INT`
--------------------------

Creates simple integral type.

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
