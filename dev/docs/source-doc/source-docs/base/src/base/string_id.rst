=========
String id
=========

.. contents::
	:depth: 2
	:local:

String id is a library that implements :code:`StrId` type.
It is a light-weight representation of string.

.. important::
	String id uses global data, and therefor should not be used "before" :code:`main`. It may lead to static Initialization Order Fiasco.

Functionalities
===============

StrId creation
--------------

:code:`StrId` can be created from various other string representations using :code:`StrId` constructors.

Bad state of StrId
------------------

:code:`StrId` default constructor leaves it in "bad" state, witch does not represent any string. Whether :code:`StrId` is in bad o good state can be checked using :code:`.isGood()` and :code:`.isBad()` methods.

Retrieving original string
--------------------------

String represented by :code:`StrId` can be accessed directly using :code:`.view()`, :code:`.strView()` and :code:`.str()` methods.

.. note::
	:code:`.str()` constructs a new string, while other methods provide only view to existing data.


Testing for equality
--------------------

Using :code:`==` operator on :code:`StrId` is equivalent of testing equality of represented strings (Same for :code:`!=`).

.. note::
	:code:`StrId == StrId` is extremely quick.


Comparison
----------

Using :code:`<` operator on :code:`StrId` will provide well behaving linear order. It can be used with for example :code:`std::map`.

.. important::
	Order used by :code:`<` is arbitrary and does not have anythings to do with lexicographical comparison. 


Additional functionalities
--------------------------

* :code:`StrId` can be converted to :code:`usize` representation. It is mostly for debug or strange quick hacks.
* :code:`StrId` can be hashed using standard :code:`std::hash`.
* :code:`StrId` works with :code:`base::strConcat` (:doc:`str_concat`).


Usage
=====

.. @TOOD
.. code-block:: cpp
	:caption: Example
