==================
Stringifyable enum
==================

.. contents::
	:depth: 2
	:local:

This library provides a simple way of creating :code:`enum class` types, that can be automatically converted to :code:`base::StrId` and vice versa.

Functionalities
===============

MAKE_STRINGIFYABLE_ENUM
-----------------------

Macro generating enum. It takes three main parameters: namespace, base type, enum name. All other parameters are treated as enum members.

.. important::
	For technical reasons :code:`MAKE_STRINGIFYABLE_ENUM` must be used in top-level code only. That is why :code:`namespace` parameter exist. It states in what namespace enum will be created.

Conversion to and from :code:`StrId`
------------------------------------

:code:`base::enumToStr`
^^^^^^^^^^^^^^^^^^^^^^^

See `Usage`_.

:code:`base::strToEnum`
^^^^^^^^^^^^^^^^^^^^^^^

See `Usage`_.

Usage
=====

.. code-block:: cpp
	:caption: Example

	#include <base/ints.hpp>
	#include <base/string_id.hpp>
	#include <base/stringifyable_enum.hpp>
	
	#include <iostream>

	MAKE_STRINGIFYABLE_ENUM(N, u16, MyEnum,
		A, B, C
	)

	int main() {
		N::MyEnum enum_value = N::MyEnum::A;

		std::cout << base::enumToStr(enum_value).strView() << "\n"; // "A"
		enum_value = base::strToEnum(base::StrId("B"));
	}

