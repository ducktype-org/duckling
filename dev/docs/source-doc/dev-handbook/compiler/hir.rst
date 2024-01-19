==================
HIR inner workings
==================

.. contents::
    :depth: 2
    :local:

HIR representation is a code representation used during HIR transformation, that is transformation that produce MIR.

HIR representation
==================

HIR representation consist of three main components:

* Set of symbols.
* Three-like structure of scopes.
* HIR Code representation (@TODO: not figured out yet).

HIR Symbol
---------------

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

HIR Scope
---------

@TODO

HIR implementation structure
============================

HIR consists of three main components:

* Symbol Table -- responsible for storing symbols, handling scopes, and performing lookup within scopes.
* Rift Symbols -- implementation of concrete symbols.
* Analysis State -- effectively a current state of HIR transformation.

