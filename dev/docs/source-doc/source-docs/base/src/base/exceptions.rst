==========
Exceptions
==========

.. contents::
	:depth: 2
	:local:

Exceptions is simple extension of standard C++ exception system.
It should be used everywhere.

All exception created by us should inherit from :code:`base::Exception`.
Additionally this module provides :code:`base::Panic` exception, and :code:`CORE_ASSERT`, and :code:`CORE_PANIC` macro, that should be used instead of things like :code:`<cassert>`.

Code Doc
===============

.. doxygenclass:: base::Panic

.. doxygenclass:: base::Exception

.. doxygenclass:: base::LogicError

.. doxygenclass:: base::NotYetImplemented

.. doxygendefine:: CORE_ASSERT

.. _duckling-panic-ref:

.. doxygendefine:: CORE_PANIC


