=========
String id
=========

.. contents::
	:depth: 2
	:local:

String ID is a library that implements ``StrId`` type.
It is a light-weight representation of string.

.. important::
	String id uses global data, and therefor should not be used "before" ``main``. It may lead to static Initialization Order Fiasco.

Functionalities
===============

StrId creation
--------------

``StrId`` can be created from various other string representations using ``StrId`` constructors.

Bad state of StrId
------------------

``StrId`` default constructor leaves it in "bad" state, witch does not represent any string. Whether ``StrId`` is in bad o good state can be checked using ``.isGood()`` and ``.isBad()`` methods.

Retrieving original string
--------------------------

String represented by ``StrId`` can be accessed directly using ``.view()``, ``.strView()`` and ``.str()`` methods.

.. note::
	``.str()`` constructs a new string, while other methods provide only view to existing data.


Testing for equality
--------------------

Using ``==`` operator on ``StrId`` is equivalent of testing equality of represented strings (Same for ``!=``).

.. note::
	``StrId == StrId`` is extremely quick.


Comparison
----------

Using ``<`` operator on ``StrId`` will provide well behaving linear order. It can be used with for example ``std::map``.

.. important::
	Order used by ``<`` is arbitrary and does not have anythings to do with lexicographical comparison. 


Additional functionalities
--------------------------

* ``StrId`` can be converted to ``usize`` representation with ``strIdToNum``. It is mostly for debug or strange quick hacks.
* ``StrId`` can be hashed using standard ``std::hash``.
* ``StrId`` works with ``base::strConcat`` (:doc:`str_utils`).

Code doc
========

.. doxygenclass:: base::StrId

.. doxygenfunction:: base::strIdToNum
