===============
HELIOS Pipeline
===============

.. toctree::
	:caption: HELIOS Pipeline:
	:titlesonly:
	:glob:


High-level
__________

HELIOS starts by taking as an input a path to a module and constructs HOUT using QueryFramework.


Low-level
_________

HELIOS begins with a query ``QueryHOUT``, that takes whole project as an input.
Inside of this query, a root scope of a main module is queried with ``QueryRootScopeOf``.
It gives it access to symbols inside a given module and everything needed for further computations.

With root scope in hand, it then queries all symbols in a module with ``QuerySymbolsInScope``.
Next, it iterates over symbols and finds functions, classes, consts, imports and other definitions.
To calculate values of constants using ``QueryConstValueOf``.

If a module imports another module, it is accomplished through a ``using`` symbol, so from HELIOS'es
point of view, all lookups are performed as if it was all in a single file.
