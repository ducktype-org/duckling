======
Logger
======

.. contents::
    :depth: 3
    :local:

Module implementing logging for source code (with location, error structure, etc.).

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/logger/index.rst

Usage example
=============

.. code-block:: cpp
    :caption: Basic usage example

    // Extend Error, Warning, or Info.
    class MessageRelevantToThisSituation: public Error {
        // ...

    protected:
        dia::Message::Domain getDomain() const override { /* ... */ }

        printer::MessageContent toMessageContentBrief() const override {
            // ...
        }
    };

    // ...

    dia::Logger logger;

    // ...
    
    dia::SourcePosition currentPosition = {...};
    if (<check-if-error-in-current-position>) {
        logger.log(base::make_unique<MessageRelevantToThisSituation>( /* ... */ ));
    }

    // ...

    if (logger.bad()) {
        bool detailed = false;
        errorState.dumpLog(detailed); // prints errors with file, position, part of code, etc.
        exit(1);
    }

Interface
=========

All symbols are in namespace ``dia``.

Error State
-----------

Logger is a class that keeps track of errors, warnings, and other messages and stores them for future output.

Adding errors
^^^^^^^^^^^^^

New errors are added using the ``log()`` method.
Every message must extend the ``Error``, ``Warning``, or ``Info`` class and implement the ``getDomain()`` and ``getBaseMessageContent()`` methods.
The first one treats it as a fatal error while the second treats it more like a warning. There are two versions for both. One takes position and message while the other takes only message.

Printing errors
^^^^^^^^^^^^^^^

Errors are printed to output using the ``dumpLog()`` method.

Checking state
^^^^^^^^^^^^^^

Methods ``good()``, ``bad()`` and ``messageCount()`` are used to get information about the current number of messages.
The ``messageCount()`` method can take a severity as an argument and thus can be used to count errors and warnings.
