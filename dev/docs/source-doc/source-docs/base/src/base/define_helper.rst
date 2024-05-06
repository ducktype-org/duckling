=============
Define helper
=============

.. contents::
	:depth: 2
	:local:


Define helper is a set of functionalities commonly used in macro programming.

Functionalities
===============

.. doxygendefine:: CONCAT

.. doxygendefine:: CONCAT_2

.. doxygendefine:: COMMA


If-s
----

If-s macros allow for simple conditional compilation. :code:`IF(true, A, B)` will expand o :code:`A`, :code:`IF(false, A, B)` will expand to :code:`B`. Analogously for :code:`IF_NOT`.

.. doxygendefine:: IF

.. doxygendefine:: IF_NOT


Diagnostics
-----------

Push/pop diagnostics allows to push/pop diagnostic options via pragmas with acts like diagnostic scope.
If a diagnostic option is changed using :code:`_Pragma` inside push/pop pair, it will only affect code inside this pair.

.. note:: Doxygen does not see those macros for some reason. Probably because they are inside if-s.

Usage
^^^^^^^^^^^^^^^

.. code-block:: cpp

	PUSH_DIAGNOSTIC
	NO_SHADOW	
	// shadowed declarations are ignored here
	POP_DIAGNOSTIC