==========
Str concat
==========

.. simple-description::

.. contents::
	:depth: 2
	:local:

Provides an convenient way of concatenating string of various representations into one.

Functionalities
===============

:code:`base::strConcat`
-----------------------

.. doxygenfunction:: base::strConcat

Usage
=====

.. code-block:: cpp
	:caption: Example

	#include <base/str_concat.hpp>
	#include <base/string_id.hpp>
	#include <iostream>

	int main() {
		base::StrId str("def");

		// prints: abc4def true
		std::cout << base::strConcat("abc", 4, str, " ", true, "\n");
	}



