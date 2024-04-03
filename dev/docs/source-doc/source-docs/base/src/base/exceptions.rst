==========
Exceptions
==========

.. simple-description::

.. contents::
	:depth: 2
	:local:


Exceptions is simple extension of standard C++ exception system.
It should be used everywhere.

All exception created by us should inherit from :code:`base::Exception`.
Additionally this module provides :code:`base::Panic` exception, and :code:`RIFT_ASSERT`, and :code:`RIFT_PANIC` macro, that should be used instead of things like :code:`<cassert>`.

Functionalities
===============

.. note:: Docs should be added inside source. Also this section is temporary.

.. doxygenclass:: base::Panic

.. doxygenclass:: base::Exception

.. doxygenclass:: base::LogicError

.. doxygenclass:: base::NotYetImplemented

.. doxygendefine:: RIFT_ASSERT

.. _rift-panic-ref:

.. doxygendefine:: RIFT_PANIC


