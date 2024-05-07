====
Ints
====

.. important::
	:code:`<cstdint>` should not be used, unless necessary. Ints should be used instead.

Description
===========

Ints is a library analogous to :code:`<cstdint>` with generally shorter type names and one big improvement: :code:`u8` and :code:`i8` types are strongly typed. 

Types
=====

* :code:`u8`, ..., :code:`u128` -- unsigned integers
* :code:`i8`, ..., :code:`i128` -- signed integers
* :code:`byte` -- byte
* :code:`usize` -- analogous of :code:`size_t`
* :code:`uchar` -- :code:`unsigned char`
  