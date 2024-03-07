=============
Lookup result
=============

Lookup result is a micro-sub-module of HIR responsible for implementing classes representing the result of "lookup". Because of features like inheritance, overrides or :code:`using a.*`, lookup result is not simple structure. Each lookup result of single :code:`.` operator is a tree like structure where each non-leaf node represent something like :code:`using a.*`, while each leaf node represent actual symbol that was found.

Lookup result classes
=====================

.. doxygenstruct:: symtable::LookupResult
.. doxygenstruct:: symtable::LookupNode
.. doxygenstruct:: symtable::ChainLookupResult
.. doxygentypedef:: symtable::SymbolChain


Additional Functions
====================

.. doxygenfunction:: symtable::dprintSymbolChain

.. doxygenfunction:: symtable::deAliasSymbolChain



