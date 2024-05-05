==============
Smart pointers
==============

Description
===========

Smart pointers are useful for resource management. Rather than use `std::unique_ptr` we use our extended version in `base::unique_ptr` that adds the ability to create a `base::borrow_ptr` from it that can access(and possibly modify) the underlying value. If the original `base::unique_ptr` is deleted then all `base::borrow_ptr` that were borrowing from it are invalid and using them will lead to undefined behavior.

Usage
=====

.. code-block:: cpp
	:caption: Example

	#include <base/smart_pointers.hpp>

	// we know we don't own `borrowed`:
	void borrows(base::borrow_ptr<int> borrowed) {
		*borrowed++;
	}

	int main() {
		base::unique_ptr<int> i = base::make_unique<int>(0);
		
		// no need fot .get():
		borrows(i.borrow_mut());
	}

Interfaces
==========

.. doxygenclass:: base::unique_ptr
	:members:
	:undoc-members:

.. doxygenfunction:: base::make_unique

.. doxygenclass:: base::borrow_ptr
	:members:
	:undoc-members:

.. doxygentypedef:: base::c_borrow_ptr

