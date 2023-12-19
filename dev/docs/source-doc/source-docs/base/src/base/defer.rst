=====
Defer
=====

.. simple-description::

Defer is a macro that postpones execution of expression till the end of scope.

.. attention:: Multiple defer statements can not be used in the same line.

.. contents::
	:depth: 2
	:local:

Usage
=====

.. code-block:: cpp
	
	#include <base/defer.hpp>

	int main() {
		{
			defer (a++);
		}
	}


Code doc 
========

.. This should be moved to different file probably

.. doxygendefine:: defer
