====
Ints
====

.. simple-description::

.. important::
	:code:`<cstdint>` should not be used, unless necessary. Ints should be used instead.

Ints is a library analogous to :code:`<cstdint>` but with one main improvement: :code:`u8` and :code:`i8` types are strongly typed. 

Functionalities
===============

Types
-----

* :code:`u8`, ..., :code:`u128` -- unsigned integers
* :code:`i8`, ..., :code:`i128` -- signed integers
* :code:`byte` -- byte
* :code:`usize` -- analogous of :code:`size_t`
* :code:`uchar` -- :code:`unsigned char`
  