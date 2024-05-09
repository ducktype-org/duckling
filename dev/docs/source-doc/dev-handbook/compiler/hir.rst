==================
HIR inner workings
==================

.. deprecated:: 9.05.2024
    THIS entire file contains LEGACY information.
    It will be updated as a follow-up to mission 36.


.. attention:: This section will need to be expanded in the future.

.. contents::
    :depth: 2
    :local:

HIR representation is a code representation used during HIR transformation, that is the transformation that takes PST and produce MIR.

HIR representation
==================

HIR representation consist of three main components:

* A set of symbols.
* Tree-like structure of scopes.
* HIR Code representation (@ not figured out yet).

HIR Symbols and symbol interface
--------------------------------

Every HIR Symbol has following properties (not necessarily calculated during initialization):

* Kind -- general classification of the symbol
* Name -- string or wildcard marking.
* Scope -- id of the scope within which it is defined.
* Type -- type in the sense of the Type System.
* Value -- type in the sense of a Compile Time Value.

Additionally every HIR Symbol needs to implement the following requests (each request might spawn an entire compilation processes if necessary):

* Lookup Request -- lookup within the symbol (e.g. by :code:`.` operator).
* Value Request
* Type Request
* DeAlias Request -- changing of the alias-like symbols into concrete symbols
* @TODO: more?

Is most cases the implementation of a different HIR-symbols will be very similar, as for example in most cases lookup request would simply forward the lookup to some scope. But in general each symbol might have its own and unique behaviors. For example lookup inside a variable will forward the lookup request to its type, which would then proceed accordingly.

HIR Scope
---------

HIR-Scope is a simple structure that holds a set of symbols inside a single scope (in most cases scope is represented by :code:`{...}` in the source code).
HIR-Scope forms a tree-like structure, that is every scope (excluding artificial root scope) has its parent.

Additionally HIR-Scope has two main states: 

* Open scope -- allows for addition of symbols, but does not allow for lookup.
* Closed scope -- allows for lookup, but that not allow for addition of symbols.

Once the scope is closed it can never be opened.

HIR implementation structure
============================

HIR consists of three main components:

* Symbol Table -- responsible for storing symbols, handling scopes, and performing lookup within scopes.
* Rift Symbols -- implementation of concrete symbols.
* Analysis State -- effectively the current state of HIR transformation.

