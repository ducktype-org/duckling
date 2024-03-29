===========
Init guards
===========

.. contents::
	:depth: 2
	:local:

Intention of this library is to provide automatic way of detecting if initialization function caused cycle (either Panic should be thrown or reinit should be prevented), and to detect if initialization function was already called before.

.. important::
	Current implementation just ignores secondary inits after the first finished.

Code doc 
========

.. doxygendefine:: RIFT_SIMPLE_INIT_GUARD_BEGIN

.. doxygendefine:: RIFT_SIMPLE_INIT_GUARD_END