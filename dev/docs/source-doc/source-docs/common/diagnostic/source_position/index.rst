===============
Source Position
===============

.. contents::
    :depth: 3
    :local:

Module implementing source position handling.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:
    
    src/source_position/index.rst

Usage example
=============

.. code-block:: cpp
    :caption: Basic usage example

    int main() {
        dia::SourcePosition singleCharacter{<source-file>, <line>, <column>, <position-in-file>};
        dia::SourcePosition multipleCharacters{<source-file>, <line>, <column>, <start-position>, <end-position>};
        dia::SourcePosition rangeFromSingleCharacter{singleCharacter, <end-position>};

        std::cerr << multipleCharacters.genErrorStr("some message") << "\n";
    }

Interface
=========

All symbols are in namespace ``dia``.

``SourcePosition`` are checked to be valid during construction and are immutable afterwards.

getSourceChars
^^^^^^^^^^^^^^

This method allows to extract a string containing the characters in the given position.

genErrorStr and genErrorMsg
^^^^^^^^^^^^^^^^^^^^^^^^^^^

These methods allow to construct an error string/message about this position in the source code with the reason passed as an argument.