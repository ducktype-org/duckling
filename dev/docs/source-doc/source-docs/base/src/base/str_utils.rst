=========
Str utils
=========

.. contents::
	:depth: 2
	:local:

Provides a convenient set of utilities for concatenating string of various representations into one,
splitting strings, and replacing strings.

Functionalities
===============

``base::strConcat``
-------------------

.. doxygenfunction:: base::strConcat

``base::strReplaceAll``
-----------------------

.. doxygenfunction:: base::strReplaceAll

``base::strSplit``
------------------

.. doxygenfunction:: base::strSplit

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
