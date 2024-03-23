===============
Query Framework
===============

This module provides implementation of Query Framework used in compiler.

.. contents::
    :depth: 2
    :local:

.. note:: In the future Query Framework might evolve, as architecture of the compiler becomes mature with features such as incremental compilation. As of today Query Framework has been designed in the way that make it easy to adapt existing concepts like "type table" or "symbol table" into it. That means that Query Framework deviates from "perfect model" in similar way that Rustc does (at least as of 2022). Some efforts has been made in order to validate that migration to more mature Query Framework will be possible in the future and that it will require reasonable amount of work. 

.. toctree::
    :caption: Code details:
    :titlesonly:
    
    src/query_framework/index.rst


General overview
================


How to write a query (with examples)
====================================

In order to create the query two things need to be implemented:

* query declaration -- placed in hpp or cpp
* query implementation -- placed in cpp

Query Declaration
-----------------

Declaring the query is very simple and requires programmer to provide three things:

* Query Name -- just a name od declaration that will represent the query in the program
* Query Key Type -- a type of valuer that query takes as an parameter
* Query Result Type  -- a type of value that query outputs

.. attention::
    Query keys need to have two critical functionalities: they need to be copyable, they need to implement :code:`std::hash` in the way that is per-query-collision free
    See: :ref:`qkey-requirements`.

.. literalinclude:: example/decl.hpp
    :caption: Query Declaration
    :language: cpp

.. note:: It is greatly advised that query declaration header files only include :code`query_int.hpp` file, as it provides the minimal set of dependencies, making incremental compilation of C++ better.

Query Implementation
--------------------

Query implementation is were we will write actual query code.

Full example is included bellow, here is a step by step guide:

In order to create such implementation one must first include the query declaration as well as :code:`query_framework/query_impl.hpp`:

.. code-block:: cpp

    #include <query_framework/query_impl.hpp>
    #include "decl.hpp" // query declaration

After that we can move to actual query implementation.
This is archived by creating a :code:`struct` called "implementation struct" that will inherit from :code:`query::QueryImplementation`.
:code:`query::QueryImplementation` is a template that takes two arguments: query to implement and :code:`PResult`.
After the definition of implementation struct one must also write the magic line presented in the example bellow.

.. note::
    Name the implementation :code:`struct` can be arbitrary but as a convention one should use: :code:`ImplementationOf_QUERY-NAME`.

.. code-block:: cpp

    struct PResult {/* ... */} // often the same as QResult

    struct ImplementationOf_MyQuery: query::QueryImplementation<
        Query2,
        PResult
    > {
        /* ... */
    }
    /**
     * Magic line (important!):
     * First argument is the name of implementation struct.
     * Second argument is the "pretty name" of the query, that will be used in logs
     * and similar places.
    */
    QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_MyQuery, "Query 2");

After all of the above is done the only thing left is to write query :code:`provide`, :code:`load` and :code:`store` functions.
This is done by creating three static methods inside implementation struct will signatures exactly the same as in the example bellow:

.. code-block:: cpp

    struct PResult {/* ... */} // often the same as QResult

    struct ImplementationOf_MyQuery: query::QueryImplementation<
        Query2,
        PResult
    > {
        	static auto provide(Context& context, QKey key) -> PResult {
                /* ... */
            }
            static auto load(QKey key) -> LoadResult {
                /* ... */
            }
            static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
                /* ... */
            }
    }
    QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_MyQuery, "Query 2");

 
Full example
++++++++++++

abc

Important concepts
==================


Requirements of queries 
=======================


.. _qkey-requirements:

Requirements of Key types
-------------------------

