===========
Error State
===========

.. contents::
    :depth: 3
    :local:

Module implementing error handling for source code (with location, error structure, etc.).

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/error_state/index.rst

Usage
=====

.. code-block:: cpp
    :caption: Basic usage

    dia::ErrorState errorState;

    ...
    
    dia::SourcePosition currentPosition = {...};
    if (<check-if-error-in-current-position>) {
        errorState.failAndLog(currentPosition, "some relevant error message");
    }

    ...

    if (errorState.fail()) {
        errorState.dumpLog(); // prints errors with file, position, part of code, etc.
        exit(1);
    }

Interface
=========

All symbols are in namespace ``dia``.

Error State
-----------

Error State is a class that keeps track of errors and stores them for future output

Adding errors
^^^^^^^^^^^^^

New errors are added using one of the ``failAndLog`` or ``logError`` methods. The first one treats it as a fatal error while the second treats it more like a warning. There are two versions for both. One takes position and message while the other takes only message.

Printing errors
^^^^^^^^^^^^^^^

Errors are printed to output using ``dumpLog()`` method.

Checking state
^^^^^^^^^^^^^^

methods ``good()``, ``fail()`` and ``errCount()`` are used to get information about the current number of errors.
