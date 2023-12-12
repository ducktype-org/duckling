==============
Borrow pointer
==============

.. simple-description::

:code:`borrow_ptr` is a special type of smart pointer that works with :code:`unique_ptr`. :code:`borrow_ptr` can be created from :code:`unique_ptr` using :code:`.borrow()` and :code:`.borrow_mut()` methods.

Intuitively :code:`borrow_ptr` allows to borrow from :code:`unique_ptr` in a way that is semantically meaningful. :code:`borrow_ptr` does not allow to delete an object underneath.

.. attention::
	Programmer must ensure himself that lifetime of :code:`borrow_ptr` does not exceed corresponding :code:`unique_ptr`.

Usage
=====

.. code-block:: cpp
	:caption: Example

	#include <base/unique_ptr.hpp>
	#include <base/borrow_ptr.hpp>

	// we know we don't own `borrowed`:
	void borrows(base::borrow_ptr<int> borrowed) {
		*borrowed++;
	}

	int main() {
		base::unique_ptr<int> i = base::make_unique<int>(0);
		
		// no need fot .get():
		borrows(i.borrow_mut());
	}

