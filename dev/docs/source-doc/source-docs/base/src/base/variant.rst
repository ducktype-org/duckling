=======
Variant
=======

.. simple-description::

This is a wrapper for :code:`std::variant` with additional functionalities.

Functionalities
===============

:code:`std::variant`
--------------------

Variant match
-------------

Variant match is a macro that allows to match over :code:`std::variant` types.

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
			variant_default_case {
				// executes if non other does
			}
		}
	}


