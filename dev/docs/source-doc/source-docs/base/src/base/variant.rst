=======
Variant
=======

.. contents::
	:depth: 2
	:local:

Implements additional ``std::variant`` functionalities. 

Functionalities
===============

Variant visit
-------------

Variant visit provides macros that simplify ``std::visit`` calls.

.. doxygendefine:: VARIANT_VISIT

.. doxygendefine:: VISIT_CASE

.. doxygendefine:: VISIT

Usage
^^^^^

.. code-block:: cpp

	#include <base/variant.hpp>

	int main() {
		std::variant<int, bool, char> variant;
		
		std::cout << VARIANT_VISIT(variant,
			VARIANT_CASE(int&, i, return i++)
			VARIANT_CASE(bool, b, return int(b))
			VARIANT_CASE(char, c, return int(c))
		) << "\n";

		std::cout << VISIT(variant, aut, return int(aut)) << "\n";
	}

Variant match
-------------

.. note::
	This functionality is macro based. Braces are very important for it to work properly.

Variant match is a macro that allows to match over :code:`std::variant` types.

.. doxygendefine:: variant_match
	:outline:

.. doxygendefine:: variant_case

.. doxygendefine:: variant_case_novalue

.. doxygendefine:: variant_default

Usage
^^^^^

.. code-block:: cpp

	#include <base/variant.hpp>

	int main() {
		std::variant<int, bool, char> variant;
		
		variant_match (variant) {
			variant_case (int, v_i) {
				// use v_i as int
			}
			variant_case_novalue (char) {
				// do some stuff if variant holds char
			}
			variant_default {
				// executes if non other does
			}
		}
	}


