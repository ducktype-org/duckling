===============
Query Framework
===============

This module provides implementation of Query Framework used in compiler.

.. contents::
    :depth: 3
    :local:

.. note:: In the future Query Framework might evolve, as architecture of the compiler becomes mature with features such as incremental compilation. As of today Query Framework has been designed in the way that make it easy to adapt existing concepts like "type table" or "symbol table" into it. That means that Query Framework deviates from "perfect model" in similar way that Rustc does (at least as of 2022). Some efforts has been made in order to validate that migration to more mature Query Framework will be possible in the future and that it will require reasonable amount of work. 

.. toctree::
    :caption: Code details:
    :titlesonly:
    
    src/query_framework/index.rst

.. attention::
    The following sections of this document form a guide for users of query framework.
    For Query Framework developer guide see (@TODO: dev-guide does not yet exist, but comments inside the code are nice!).

General overview
================

This framework is based on a proposal that can be found here: (@TODO: link once the proposal is merged into dev-space).

Query Model is a way of writing a compiler. At the core of this model there are queries.
Each query is somewhat independent piece of code that is responsible for given piece of compilation.
As en example some queries that might be present in the compiler are:

* Query for getting type of a symbol.
* Query for getting optimized version of a piece of code.
* Query for looking up a name within some scope.

Each query to compute its output can call other queries which creates sort of dependency graph between query calls (this graph is actually explicitly created by the Query Framework!).
Such model has several advantages:

* It creates a one, enforced of communication between modules in the compiler. That means that whenever someone wants to use some other module, going through public queries of that module should be enough to learn it's interface.
* Inherent dependency tracking of query based architecture will greatly simplify incremental compilation in the future.
* Each query computes only what is truly needed to get its result. That means that compiler can work as a language server and respond quickly when IDE requests some information.
* It allows for early phases of the compiler to use results of latter phases (as for example: lookup requiring comp-time evaluation to expand a macro).


What Query Framework does
-------------------------

Query Framework is responsible for following things:

* **Query declaration and implementation base** -- Query Framework provides tools to easily declare and implement queries in the "one-correct-way".
* **Dependency tracking and cycle detection** -- Query Framework automatically tracks dependencies in the back and will report when query calls will create a cycle.
* **Error reporting** -- Query Framework formalize how errors should be reported. It does not however implement an error/diagnostic class itself.

In the future Query Framework will also implement big part of incremental compilation.

.. _s-psl-model:

Side-effect Provide Load Store Model
====================================

In current implementation Query Framework implements co called "Side-effect Provide Load Store Model" (S-PSL).

.. attention::
    In S-SPL model, as the name suggest, we allow queries to have side-effects, although they should be controlled, and it is encouraged to write pure queries when possible.

Each query in this model consist of following main components:

* :code:`QKey` -- Query Key, a data type that will be the query argument. This type has to follow some rules described in: :ref:`qkey-requirements`.
* :code:`QResult` -- Query Result, a data type returned from the query.
* :code:`PResult` -- Provider Result, a data type returned from the query provider function. This type will more often then not be equal (or almost equal) to :code:`QResult`. Purpose of :code:`PResult` is just to simplify code of Query Providers.
* :code:`provide` function -- Query Provider, a function with signature similar to :code:`QKey -> PResult`. This function performs actual computation that calculates query result and perform compilation steps.
* :code:`store` function -- A function with signature similar to :code:`QKey, PResult -> QResult`. This function stores result of provider computation in some cache/table/database and returns final Query result.
* :code:`load` function -- A function with signature similar to :code:`QKey -> QResult?`. Function that checks if Query result is not present in cache already. It returns optional holding Query Result.

.. note:: Actual type of provide, load and store functions are slightly different and are explained in sections related to writing them.

Existence of user defined store and load function means that each query can implement caching in its own unique way. From one point of view this can be bug prone, but from the other point this means that queries can be very easily adapted as interfaces of existing modules. For example Type System can treat "type table" as its internal cache shared among many queries.

Deviation from ideal and pure query model means that programmes has to ensure by hand that some "laws" are always obeyed. List of those "laws" can be fount here: :ref:`all-requirements`.


How to write a query (with examples)
====================================

.. attention::
    This section assumes you read :ref:`s-psl-model`.

In order to create the query two things need to be implemented:

* query declaration -- placed in hpp or cpp
* query implementation -- placed in cpp

Query Declaration
-----------------

Declaring the query is very simple and requires programmer to provide three things:

* Query Name -- just a name od declaration that will represent the query in the program
* Query Key Type (:code:`QKey`) -- a type of valuer that query takes as an parameter
* Query Result Type (:code:`QResult`) -- a type of value that query outputs

.. attention::
    Query keys need to have two critical functionalities: they need to be copyable,
    they need to implement :code:`std::hash` in the way that is per-query-collision free.
    See: :ref:`qkey-requirements` for more details.

.. literalinclude:: example/decl.hpp
    :caption: Query Declaration
    :language: cpp

.. note:: It is greatly advised that query declaration header files only include :code`query_int.hpp` file, as it provides the minimal set of dependencies, making incremental compilation of C++ better.

Query Implementation
--------------------

Query implementation is were we will write actual query code.

Full example is included bellow, here is a step by step guide:

Including dependencies
++++++++++++++++++++++

In order to create such implementation one must first include the query declaration as well as :code:`query_framework/query_impl.hpp`:

.. code-block:: cpp

    #include <query_framework/query_impl.hpp>
    #include "decl.hpp" // query declaration

Writing implementation boilerplate
++++++++++++++++++++++++++++++++++

After that we can move to actual query implementation.
This is archived by creating a :code:`struct` called "implementation struct" that will inherit from :code:`query::QueryImplementation`.
:code:`query::QueryImplementation` is a template that takes two arguments: query to implement and :code:`PResult`.
After the definition of implementation struct one must also write the magic line presented in the example bellow.

.. note::
    Name the implementation :code:`struct` can be arbitrary but as a convention one should use: :code:`ImplementationOf_QUERY-NAME`.

.. code-block:: cpp

    // Some type, often the same as QResult
    struct PResult {/* ... */}

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
This is done by creating three static methods inside implementation struct with signatures exactly the same as in the example bellow:

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

.. hint::
    It can be useful to define additional static variables inside implementation struct.
    Assuming the code is in cpp-file it can be easily done by declaring :code:`inline static` members.
 
Writing provide function
++++++++++++++++++++++++

Provide function can in general perform any computations, however programmer should make sure that it is as pure as is possible from practical point of view.

Apart from user defined :code:`QKey`, provide function takes as an argument parameter :code:`Context& context`. This parameter is very important as it allows for 3 key functionalities, that should **NEVER** be achieved otherwise:

* Calling other queries from inside a query.
* Emitting logs from inside a query.
* Reporting compilation errors from inside a query.

.. note::
    As of right now emitting logs and reporting errors is based of simply strings. In the future it will change after diagnostic framework will be created.


.. code-block:: cpp
    :caption: Provider function:

        	static auto provide(Context& context, QKey key) -> PResult {
                // ...
                
                // call other query:
                auto result = context.query<OtherQueryName>(other_query_key);

                // logging:
                context.log("Some random log.");

                // reporting errors:
		        context.compilationError("Error -- example error.");
            }

How to write auxiliary functions?
*********************************

Sometimes provider function might grow large and we would like to separate it into smaller function.
Fortunately that is not a problem. If the "auxiliary functions" does not need functionalities from :code:`Context& context`, then it can be just a standard function or static method (or any other code).
If it does need functionalities from :code:`Context& context` then it can be just a standard function or method that takes :code:`Context&` as a parameter as well.
As of right now this can be achieved by writing static method next to :code:`provide` function, like this:

.. code-block:: cpp
    :caption: Auxiliary function:

            static auto auxiliary(Context& context, u32 a) -> u64 {...}
        	static auto provide(Context& context, QKey key) -> PResult {
                // ...
                auto aux = auxiliary(context, 42);
                // ...
            }

.. note::
    as of today there is no nice way of writing auxiliary function with context parameter outside query implementation struct.
    This can be however added in the future if the need arise.


Writing load and store function
+++++++++++++++++++++++++++++++

Load and store function should be kept as minimal as possible.

There are two very simple  concepts to unravel before we can go into implementation:

* :code:`query::ACD` type -- This is just "additional cache data". Store function has to store value of this type alongside every cache entry while load function has to retrieve it.
* :code:`LoadResult` type -- This is a type that expands to :code:`base::Optional<query::AddACD<QResult> > `. :code:`Optional` comes from the fact that the load function may not find a cached value. :code:`query::AddACD<T>` is just a simple template that stores a value of type :code:`T` and a value of type :code:`query::ACD`. In other words :code:`LoadResult` type is just an optional of a pair :code:`QResult, query::ACD`.

Now we can finally write the functions:

.. code-block:: cpp

            static auto load(QKey key) -> LoadResult {
                if (/* cache miss */) {
                    return {}; // empty optional
                }
                if (/* cache hit */) {
                    return { some_data, acd };
                    // one can also use query::AddACD inside cache implementation
                    // and simply retrieve that. 
                }
            }
            static auto store(QKey key, PResult q_res, query::ACD acd) -> QResult {
                // maybe perform some simple computation arising from the fact, that
                // PResult != QResult:
                
                // for example:
                QResult q_result = someStuff(res);

                // store either PResult or QResult in cache along side with acd:

                // for example:
                some_cache.store(query::AddACD{ some_result, acd });

                // return final result
                // for example:
                return q_result;
            }

.. note::
    One can also create query that is not cached. In that case :code:`load` trivializes to :code:`return {};`
    and store  trivializes to :code:`RIFT_PANIC`.
    One must however conform to :ref:`general-requirements`.


Full example
++++++++++++

@TODO


Other most important concepts
-----------------------------

Cycles
++++++


.. caution::
    As of right now when Query Framework detects cycles it just throws a :ref:`panic <rift-panic-ref>`.
    In the future versions it will report a critical compilation error.
    Even later proper handling of cyclic queries will be added.

Running queries from outside the query framework
================================================


In order to call a query from "outside" the framework one should use special function: :code:`queryEntryPoint`.
See :doc:`src/query_framework/query_entry_point` for code details.

.. code-block:: cpp
    :caption: Query call from outside example

        #include <query_framework/query_entry_point.hpp>
        #include <iostream>

        int mani() {
            std::cerr << query::queryEntryPoint<MyQuery>(some_key) << "\n";
        }


Important concepts
==================

.. _all-requirements:

Requirements of queries 
=======================

.. _general-requirements:

General Requirements of Queries 
-------------------------------

Consistency
+++++++++++

Since queries are in general not pure, programmer needs to guarantee that values returned by a query are consistent.
That means that a query called on the same key will always produce an identical result
(identical meaning either strictly identical or not distinguishable by the rest of the compiler).
In practice that means that usually all non-pure queries are correctly cached.

.. _qkey-requirements:

Requirements of Key types
-------------------------

Copyable
++++++++

Every key type will be copied around by the framework.
Programmer has to ensure that copy operation will compile and that it will not brake the state of the key or of the compiler.


Perfect Hashing
+++++++++++++++

Every key type need to implement :code:`std::hash` that is collision less.

.. attention::
    Use of :code:`std::hash` might not be the best here.
    In the future either custom "hash" system will be created or "collision less" requirement will be dropped
    (though it does greatly simplify dependency tracking).
    For ID like keys this condition is somewhat trivial to fulfill.
    It is not known how often non-ids keys will appear.

.. _provider-requirements:

Requirements of Provider functions
----------------------------------

Almost Pureness
+++++++++++++++

It is not strict requirement but side effects should be avoided unless they are really needed.

.. note:: Caching itself is obviously a side effect, but it is an expected one. 