=====
Lexer
=====

.. contents::
    :depth: 3
    :local:

Module implementing the conversion from source code to tokens using keyword definitions provided by ``rift_definitions`` module.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/lexer/index.rst
    tests.rst

Usage
=====

.. code-block:: cpp
    :caption: Basic usage

    #include<lexer/lexer.hpp>
    #include<rift_definitions/key_spec_op.hpp>

    int main() {
	    lexer::init();
	    rift_def::setKeywordMode(rift_def::KeywordMode::RiftSource);

	    fs::FilePath file("path/to/rift/file");
	    lexer::TokenData td = lexer::tokenizeFile(file);
    }

If decoding or tokenization fails then ``lexer::tokenizeFile`` function prints error to ``std::cerr`` and throws a ``base::LogicError``.

Interface
=========

All symbols are in namespace ``lexer``.

The main ways of interfacing with the lexer are in files ``lexer.hpp`` and ``token.hpp``.

lexer.hpp
---------

``lexer.hpp`` contains the interface needed to run the lexer.

init
^^^^

Initializes resources that are needed to run the lexer.

.. code-block:: cpp

    void init();

tokenizeFile
^^^^^^^^^^^^

Returns a tokenization of a given file or outputs errors to ``std::cerr`` and throws ``base::LogicError``

.. code-block:: cpp

    TokenData tokenizeFile(fs::FilePath file);

token.hpp
---------

``token.hpp`` contains the interface with the structures returned by lexing.

TokenData
^^^^^^^^^

The result of lexing. It contains: 

#. ``Tokens tokens`` - List of top level tokens.
#. ``Token eof_sentinel`` - Token that can be used as EOF.
#. ``fs::FileContent file_content`` - Pointer to the contents of the source file.

Token
^^^^^

The building block of the result of lexing. It keeps all of the important information about the properties of a given token and has an interface(shown in the source code section) that allows to check against that information.

It includes two enums:

#. ``Type`` - a general type of a token 
#. ``BracketType`` - type describing different bracket types.

