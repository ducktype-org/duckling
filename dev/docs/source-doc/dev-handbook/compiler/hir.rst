==================
HIR inner workings
==================

.. attention:: This section will need to be expanded in the future.

.. contents::
    :depth: 2
    :local:

HIR representation is a code representation used during HIR transformation, that is the transformation that takes PST and produce MIR.

HIR representation
==================

HIR representation consist of three main components:

* Set of symbols.
* Three-like structure of scopes.
* HIR Code representation (@ not figured out yet).

HIR Symbols and symbol interface
--------------------------------

Every HIR Symbol has following properties (not necessarily calculated during initialization):

* Kind -- general classification of the symbol
* Name -- string or wildcard marking.
* Scope -- id of scope within it is defined.
* Type -- type in the sense of Type System
* Value -- type in the sense of Compile Time Value

Additionally every HIR Symbol need to implement following requests (each request might spawn entire compilation processes if necessary):

* Lookup Request -- lookup with in the symbol (e.g. by :code:`.` operator).
* Value Request
* Type Request
* DeAlias Request -- changing of alias like symbols into concrete symbols
* @TODO: more?

Is most cases implementation of a given HIR-symbol will be vary similar, as for example in most cases lookup request would simply forward the lookup to some scope. But in general each symbol might have its own and unique behaviors. For example lookup inside a variable will forward the lookup request to its type, that would then proceed accordingly.

HIR Scope
---------

HIR-Scope is a simple structure that holds a set of symbols inside a single scope (in most cases scope is represented by :code:`{...}` in the source code).
HIR-Scope form a tree-like structure, that is every scope (excluding artificial root scope) has its parent.
Additionally HIR-Scope has two main states: 

* Open scope -- allows for addition of symbols, but does not allow for lookup.
* Closed scope -- allows for lookup, but that not allow for addition of symbols.

Once the scope is closed it can never be opened.

HIR implementation structure
============================

HIR consists of three main components:

* Symbol Table -- responsible for storing symbols, handling scopes, and performing lookup within scopes.
* Rift Symbols -- implementation of concrete symbols.
* Analysis State -- effectively a current state of HIR transformation.

