=======
Message
=======

.. contents::
    :depth: 3
    :local:

Module implementing a standardised compiler message interface.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:
    
    src/message/index.rst

Usage
=====

.. code-block:: cpp
    :caption: Basic usage

    // Extend Error, Warning, or Info.
    class MyMessage: public Error {
        // ...

    protected:
        dia::Message::Domain getDomain() const override { /* ... */ }

        printer::MessageContent toMessageContentBrief() const override {
            // ...
        }
    };

Interface
=========

All symbols are in namespace ``dia``.

Message
-------

The ``Message`` is the base class for all compiler-generated messages.

The compiler reports errors via the ``dia::Logger``, which consumes ``Message`` objects.

Each ``Message`` must be supplied with a ``dia::SourcePosition``.

The ``Message`` class is abstract, but one should not derive from it directly. Instead, one should always
inherit after the ``Error``, ``Warning``, or ``Info`` abstract classes. Then, two or three methods need to be implemented.

First, is the ``getDomain()`` method. It's straight forward. Simply indicate which ``dia::Message::Domain`` the message
pertains to. If no domain suits your needs and it is reasonable to add a new domain, feel free to do so.

Second, is the ``toMessageContentBrief()`` method. It describes the cause of the message as briefly as possible, while
providing the user with enough information to eliminate the error, e.g. "Redeclaration of symbol <symbol_name>."

Third, is the optionally overridable ``toMessageContentDetailed()`` method. It behaves similarly to the ``toMessageContentBrief()``
method, but attempts to give more context, for example information useful to beginners, or examples of when the message may be thrown.
By default, it is implemented to return the exact same information as its brief counterpart.

Be careful not to override the (non-virtual) ``getBaseMessageContent(bool)`` method.

``addNote``
^^^^^^^^^^^

The ``Message`` class can also be supplied with instances of ``dia::Note`` via the ``addNote(Note) method.

Note
----

A ``Note`` is meant to supplement a ``Message`` with an additional helpful piece of information for the user.
For example e.g. it could supplement a redeclaration error with "Previous declaration here.".

Just like a ``Message``, a ``Note`` sub-class must be implemented for each case.

Unlike a ``Message``, a ``Note`` need not be supplied with a ``SourcePosition``. However, since it often will
be, an additional ``NoteWithPosition`` abstract class is available.
