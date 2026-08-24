# Query Framework
@tableofcontents

This module provides implementation of Query Framework used in compiler.

General overview
================

This framework is based on a proposal that can be found here: <link> 
@TODO link once the proposal is merged into dev-space

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
* **Incremental compilation** -- Query Framework is a backbone of the incremental compilation.


Side-effect Provide Load Store Model {#s-psl-model}
==================================== 

In current implementation Query Framework implements co called "Side-effect Provide Load Store Model" (S-PSL).

@attention
In S-SPL model, as the name suggest, we allow queries to have side-effects, although they should be controlled, and it is encouraged to write pure queries when possible.


Each query in this model consist of following main components:

* `QKey` -- Query Key, a data type that will be the query argument. This type has to follow some rules described in: @ref qkey-requirements.
* `QResult` -- Query Result, a data type returned from the query.
* `PResult` -- Provider Result, a data type returned from the query provider function. This type will more often than not be equal (or almost equal) to `QResult`. The purpose of `PResult` is just to simplify the code of Query Providers.
* `provide` function -- Query Provider, a function with a signature similar to `QKey -> PResult`. This function performs the actual computation that calculates the query result and performs compilation steps.
* `store` function -- A function with a signature similar to `(KHash), PResult -> QResult`. This function stores the result of provider computation in some cache/table/database and returns the final Query result.
* `load` function -- A function with a signature similar to `(KHash) -> QResult?`. This function checks if the Query result is not already present in cache. It returns an optional holding the Query Result.

@note 
Actual types of the `provide`, `load`, and `store` functions are slightly different and are explained in sections related to writing them.

Existence of user defined store and load function means that each query can implement caching in its own unique way. From one point of view this can be bug prone, but from the other point this means that queries can be very easily adapted as interfaces of existing modules. For example Type System can treat "type table" as its internal cache shared among many queries.

Deviation from ideal and pure query model means that programmes has to ensure by hand that some "laws" are always obeyed. List of those "laws" can be found here: @ref all-requirements.

How to write a query (with examples)
====================================

@attention
This section assumes you read @ref s-psl-model.

In order to create the query two things need to be implemented:

* query declaration -- placed in hpp or cpp
* query implementation -- placed in cpp

Query Declaration
-----------------

Declaring the query is very simple and requires the programmer to provide three things:

- Query Name -- just a name of the declaration that will represent the query in the program
- Query Key Type (`QKey`) -- a type of value that the query takes as a parameter
- Query Result Type (`QResult`) -- a type of value that the query outputs
- Set of query tags (see query tags section below) 

@attention 
Query keys need to have two critical functionalities: they need to be copyable,
and they need to implement a perfect hash (see: @ref perfect_hash.hpp) in a way that is per-query-collision free.
See: @ref qkey-requirements for more details.

@include query_framework_decl.hpp

@note It is greatly advised that query declaration header files only include `query_int.hpp` file, as it provides the minimal set of dependencies, making incremental compilation of C++ better.

Query Implementation
--------------------

Query implementation is where we will write actual query code.

Full example is included below, here is a step by step guide:

### Including dependencies

In order to create such implementation one must first include the query declaration as well as `query_framework/standard_query/query_impl.hpp`:

~~~~~cpp
#include <query_framework/standard_query/query_impl.hpp>
#include "decl.hpp" // query declaration
~~~~~

### Writing implementation boilerplate

After that, we can move to the actual query implementation. This is achieved by creating a `struct` called "implementation struct" that will inherit from `query::QueryImplementation`. `query::QueryImplementation` is a template that takes two arguments: query to implement and `PResult`. These steps are simplified by a macro `IMPLEMENT_QUERY`. After the definition of the implementation struct, one must also write the magic line presented in the example below.

@note
The implementation `struct` can be accessed somewhere else in the code by the name `ImplementationOf_{QUERY_NAME}`.

~~~~~cpp
    // Some type, often the same as QResult
    struct PResult {/* ... */};

    struct IMPLEMENT_QUERY(MyQuery, PResult) {
        /* ... */
    };
    /**
     * Magic line (important!):
     * As an argument it takes the name of the query.
    */
    QUERY_IMPLEMENTATION_BOILERPLATE(MyQuery);
~~~~~

After all of the above is done, the only thing left is to write query `provide`, `load` and `store` functions. This is done by creating three static methods inside the implementation struct with signatures exactly the same as in the example below:

~~~~~cpp
    struct PResult {/* ... */} // Often the same as QResult

    struct IMPLEMENT_QUERY(Query2, PResult) {
        static auto provide(Context& context, QKey key) -> PResult {
            /* ... */
        }
        static auto load((KHash) key) -> LoadResult {
            /* ... */
        }
        static auto store((KHash) key, PResult res, query::ACD acd) -> QResult {
            /* ... */
        }
    }
    QUERY_IMPLEMENTATION_BOILERPLATE(Query2);
~~~~~

@remark
It can be useful to define additional static variables inside the implementation struct.
Assuming the code is in a cpp-file, it can be easily done by declaring `inline static` members.

### Writing provide function

Provide function can in general perform any computations, however programmer should make sure that it is as pure as is possible from practical point of view.

Provide function takes two parameters:

- `QKey` -- Query Key of type defined in the query declaration.
- `Context& context` -- Context, a parameter provided by the framework. This parameter is very important as it allows for three key functionalities that should **NEVER** be done by other methods:

- Calling other queries from inside a query.
- Emitting logs from inside a query.
- Reporting compilation errors from inside a query.

@note
As of right now emitting logs and reporting errors is based on simple strings. In the future, this will change after the diagnostic framework is created.

Provider function:
~~~~~cpp

    static auto provide(Context& context, QKey key) -> PResult {
        // ...
        
        // call other query:
        auto result = context.query<OtherQueryName>(other_query_key);

        // logging:
        context.log("Some random log.");

        // reporting errors:
        context.compilationError("Error -- example error.");
    }
~~~~~

#### How to write auxiliary functions?

Sometimes the provider function might grow large, and we'd like to separate it into smaller functions. Fortunately, that is not a problem. If the "auxiliary functions" do not need functionalities from `Context& context`, then it can be just a standard function or static method (or any other code).

If it does need functionalities from `Context& context`, then it can be just a standard function or method that takes `Context&` as a parameter as well.

There are two ways of achieving this:

First way is to write a static method next to the `provide` function, like this:

Auxiliary function inside Query Implementation struct:
~~~~~cpp

    static auto auxiliary(Context& context, u32 a) -> u64 {...}

    static auto provide(Context& context, QKey key) -> PResult {
        // ...
        auto aux = auxiliary(context, 42);
        // ...
    }
~~~~~

Second way is to write a "Query Extension" function/method or some other code taking the context as a parameter:

Query Extension:
~~~~~cpp
    // hpp:
    output_t nameOfExtension(query::Context&, input_t);

    // cpp:
    output_t nameOfExtension(query::Context& ctx, input_t in) {
        // ...
        // you can use "ctx" here
    }

    // how to use it:
    static auto provide(Context& context, QKey key) -> PResult {
        // beware to never pass context that is not from your query:
        auto output = nameOfExtension(context, input);
    }
~~~~~

@attention
    One can technically write a Query Extension as just a function, but it is forbidden. The reason for that is to easily identify all extensions in the future during refactors.

@note
    Query Extensions can be used by multiple queries, which can help reduce code repetition.

### Writing load and store function

Load and store functions should be kept as minimal as possible.

There are two very simple concepts to unravel before we can go into implementation:

- `query::ACD` type -- This is just "additional cache data". The store function has to store a value of this type alongside every cache entry while the load function has to retrieve it.
- `LoadResult` type -- This is a type that expands to `base::Optional<query::CacheEntry<QResult>>`. `Optional` comes from the fact that the load function may not find a cached value. `query::CacheEntry<T>` is just a simple template that stores a value of type `T` and a value of type `query::ACD`. In other words, `LoadResult` type is just an optional of a pair `QResult, query::ACD`.

Now we can finally write the functions:

~~~~~cpp
    static auto load((KHash) key) -> LoadResult {
        if (/* cache miss */) {
            return {}; // empty optional
        }
        if (/* cache hit */) {
            return { some_data, acd };
            // one can also use query::CacheEntry inside cache implementation
            // and simply retrieve that.
        }
    }

    static auto store((KHash) key, PResult q_res, query::ACD acd) -> QResult {
        // maybe perform some simple computation arising from the fact that
        // PResult != QResult:
        
        // for example:
        QResult q_result = someStuff(res);

        // store either PResult or QResult in cache alongside with acd:

        // for example:
        some_cache.store(query::CacheEntry{ some_result, acd });

        // return the final result
        // for example:
        return q_result;
    }
~~~~~

@note
    One can also create a query that is not cached. In that case, `load` trivializes to `return {};`
    and `store` trivializes to simply transforming `PResult` into `QResult`.
    One must, however, conform to @ref general-requirements.

### Optional: loading results directly from disk (loadFromDisk)

For expensive queries with a stable key, you can add a small helper that tries to reuse a previously
saved result from disk without running the provider.

- Signature: `static auto loadFromDisk(const QKey& key) -> base::Optional<PResult>`

Important:
- To make reuse possible across runs, the query key must be stable (implement
    `queryStablePerfectHash`).
- A concrete example exists in the driver, but your PResult can be any type, not only file artifacts.
- The provide function should store results in a query artifact, and loadFromDisk should load the
    previously saved results.

#### Full working example

@include query_framework_decl.cpp


## Other most important concepts

### Incremental compilation

- Incremental compilation is supported: the framework builds an explicit dependency graph and
    selectively recomputes only what has changed.
- On startup, the previous query graph state is loaded from disk and dependencies are registered.
    During execution, for queries that implement a stable key and `loadFromDisk(key) -> Optional<PResult>`,
    the system automatically tries to restore the result from disk.
- Reuse occurs when the query key and all its dependencies remain the same between compilations. If
    loading fails or anything relevant has changed, the framework calls `provide(key)` and proceeds
    normally (you may then persist the new result).
- Stable hashes (`queryStablePerfectHash`) enable consistent addressing of persisted results across
    runs.

### Cycles

@attention
    As of right now when the Query Framework detects cycles, it just throws a `panic`.
    In future versions, it will report a critical compilation error.
    Even later, proper handling of cyclic queries will be added.

### Query Tags

Query framework defines a set of tags that can customize the query behavior or link some additional properties to it. Tags are set in a query declaration like so:

~~~~~cpp
    :caption: Query tags example

    DECLARE_QUERY(
        Name,
        Key,
        Result,
        ({
            /* non-default tags go here: */
            .tag1 = value,
            .tag2 = value,
            // ...
        })
    )
~~~~~

For meaning and default values of each tag refer to `query_data.hpp`.

Running queries from outside the query framework
================================================

In order to call a query from "outside" the framework, one should use a special function: `entryPoint`.
See `src/query_framework/entry/query_entry_point` for code details.

~~~~~cpp
    :caption: Query call from outside example

    #include <query_framework/entry/query_entry_point.hpp>
    #include <iostream>

    int mani() {
        std::cerr << query::entryPoint<MyQuery>(some_key) << "\n";
    }
~~~~~

Important concepts {#all-requirements}
==================

Requirements of queries {#general-requirements}
=======================

General Requirements of Queries 
-------------------------------

### Consistency

Since queries are in general not pure, the programmer needs to guarantee that values returned by a query are consistent.
That means that a query called on the same key will always produce an identical result
(identical meaning either strictly identical or not distinguishable by the rest of the compiler).
In practice, that means that usually all non-pure queries are correctly cached.

## Requirements of Key types {#qkey-requirements}

### Perfect Hashing

Every key type needs to implement `queryUnstablePerfectHash` (@ref query_hash.hpp) that is collision-less.
queryUnstablePerfectHash, the returned hash must have type either u64 or Bit256
If the key is to be cached on disk, it must additionally implement `queryStablePerfectHash` (@ref query_hash.hpp).

@attention
    For ID-like keys, perfect hashing is trivial to implement.
    For more complex situations, a trick can be used. One can use `base::HashMap` to hash-map keys to IDs.

Requirements of Provider functions {#provider-requirements}
----------------------------------

### Almost Pureness

It is not a strict requirement, but side effects should be avoided unless they are really needed.

@note Caching itself is obviously a side effect, but it is an expected one.

Auto caching
============

The Query Framework provides a way to automatically create `load` and `store` methods with
hash-map-based caching for fast prototyping.

In order to use it, two requirements must be met:

- Query key type must implement perfect hash (already a requirement of the Query Framework)
- The hash of a key must implement `operator==`, as well as `std::hash` (same as for `base::HashMap`).

There are currently two automatic-caching mechanisms:

- `QUERY_AUTO_CACHE_COPY` -- it will cache `PResult`s and return copy on load. It rely on copy constructor and move constructor of `PResult`.
- `QUERY_AUTO_CACHE_REF` -- it will cache `PResult`s and return stable references to them (in general will have slower cache, since references have to be stable)
- `QUERY_AUTO_CACHE_CONSTRUCT` -- it will cache `PResult`s and return `QResult(PResult)` on load.

It is important to ensure that it is impossible to modify cached data in any way thought QResult.

Auto cache example: by copy:
~~~~~~~~~~cpp

#include <query_framework/standard_query/query_impl.hpp>

DECLARE_QUERY(FibonacciStringAutoCache, uint64_t, std::string);

struct IMPLEMENT_QUERY(FibonacciStringAutoCache, std::string) {
    static auto provide(Context& ctx, QKey key) -> PResult {
        return std::to_string(ctx.query<Fibonacci>(Key1{ key }));
    }

    QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(FibonacciStringAutoCache);

.. code-block:: cpp
:caption: Auto cache example: by reference

#include <query_framework/standard_query/query_impl.hpp>

// Here we can return reference as it is stable:
DECLARE_QUERY(FibonacciStringAutoCache, uint64_t, CRef<std::string>);

struct IMPLEMENT_QUERY(FibonacciStringAutoCache, std::string) {
    static auto provide(Context& ctx, QKey key) -> PResult {
        return std::to_string(ctx.query<Fibonacci>(Key1{ key }));
    }

    QUERY_AUTO_CACHE_REF
};

QUERY_IMPLEMENTATION_BOILERPLATE(FibonacciStringAutoCache);
~~~~~~~~~~
