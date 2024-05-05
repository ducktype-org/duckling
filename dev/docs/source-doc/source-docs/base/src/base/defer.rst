=====
Defer
=====

Defer is a macro that postpones execution of expression till the end of scope.

.. note:: Multiple defer statements cannot be used in the same line.

.. contents::
	:depth: 2
	:local:

Usage
=====

.. code-block:: cpp
	
	#include <base/defer.hpp>

	int main() {
		int a = 3;
		// a = 3
		{
			// a = 3
			a += 2;
			// a = 5
			defer (a++);
			// a = 5
			a--;
			// a = 4
		}
		// a = 5
		{
			defer (a = 4);
			defer (a--);
			// a = 5
		}
		// a = 4;
		{
			defer (a--);
			defer (a = 3);
			// a = 4
		}
		// a = 2
	}


Code doc 
========

.. doxygendefine:: defer
