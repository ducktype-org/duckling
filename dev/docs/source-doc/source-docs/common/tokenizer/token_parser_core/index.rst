=================
Token parser core
=================

.. contents::
    :depth: 3
    :local:

Module implementing general tools and interfaces taking the output of lexer(recursive list of tokens) and preparing it for usage by a parser.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/token_parser_core/index.rst

Interface
=========

All symbols are in namespace ``tpc``.

This module can be conceptually divided into two parts:
#. Classes ``tpc::TokenStream``, ``tpc::ParserState`` and functions in `automatic.hpp` offer higher level abstractions for interacting with a list of tokens that are useful for parsing.
#. Class ``tpc::Element`` and everything from `parser_ref.hpp` and `base_element.hpp` define basic types and functions used as building blocks in abstract syntax tree

init
^^^^

``tpc::init()`` should be called before any other module usage.

ParserState
^^^^^^^^^^^

This class is the main object used for interaction from the outside, it manages a stack of ``tpc::TokenStream`` objects to keep track of recursions. Both ``tpc::ParserState`` and ``tpc::TokenStream`` are simple wrapper objects and don't keep the underlying data only a reference to it so it should be handled and held by something else.

``tpc::ParserState`` and `automatic.hpp` provide methods and functions for handling recursive tokens and higher level interactions with the current ``tpc::TokenStream`` of an ``tpc::ParserState`` object. 

Access to the token stream
--------------------------

``tokens()`` and ``ctokens()`` methods provide a way to access to the current token stream in a mutable or immutable form.

Error handling
--------------

Error handling is done through the ``dia::ErrorState err`` member which allows for complete access to current errors. It is additionally supported by the ``void fail(usize rel_pos, std::string message)`` method which adds an error to the error state relative to the current position.

Managing token streams
----------------------

``tpc::ParserState`` has a couple methods to handle its recursive streams:
#. ``bool empty()`` and ``bool notEmpty()`` check whether the current ``tpc::TokenStream`` has any tokens left.
#. ``goDown()`` adds the stream of the current recursive token to the stack as the new current stream.
#. ``goUp()`` and ``goUpAndSkip()`` remove the current stream and go back to the previous one. ``goUpAndSkip()`` additionally skips one token so that after the operation the current token is no longer the recursive one. 

Higher level single token methods
---------------------------------

``bool tryEat(<Special|Keyword|Operator> value)`` methods allow to skip a token with a particular value and return whether such a token was skipped. It's especially useful for situations where an optional token indicates a further structure for example:

.. code-block:: cpp

    if (state.tryEat(Operator::SingleArrow)) parseOne(state, &out->rets);

Parsing multiple expected elements in a row
-------------------------------------------

