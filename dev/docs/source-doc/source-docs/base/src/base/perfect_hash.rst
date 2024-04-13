============
Perfect hash
============

Provides very simple framework for defining perfect hashing for any types.

Usage
=====

In order to define perfect hash for given type :code:`T` you have to do one of:

* write :code:`t.customPerfectHash() -> base::HashT` method.
* write :code:`customPerfectHash(T) -> base::HashT` function declared in the same scope as type :code:`T`.

In order to get perfect hash of any type use :code:`base::perfectHash`.

Example
=======

.. code-block:: cpp

	#include <base/perfect_hash.hpp>

	struct T {
		base::HashT customPerfectHash() {...}
	}

	struct Q {}
	base::HashT customPerfectHash(Q) {...}

	int main() {
		base::perfectHash(T());
		base::perfectHash(Q());
	}
	

Code details
============

.. doxygenfile:: perfect_hash.hpp