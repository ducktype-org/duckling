=============
Constexpr cat
=============

.. simple-description::

.. contents::
	:depth: 2
	:local:


Constexpr cat implements a way to concatenate strings during compile time.
Is is extremely specific and strange, ans should not be used under standard circumstances.

.. note:: It is currently only used by Json library.

.. note:: For concatenation in runtime use `base::strConcat`.

Usage
=====


.. code-block:: cpp
	:caption: Example

	#include <base/constexpr_cat.hpp>
	#include <iostream>
	
	int main() {
		std::cout << CONSTEXPR_CAT("A", "B", "C");
	}