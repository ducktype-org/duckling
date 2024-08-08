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
#. Class ``tpc::Element`` and everything from `parser_ref.hpp` and `base_element.hpp` define basic types and functions used as building blocks in abstract syntax tree.

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

    tpc::ParserState& state;
    // during function parsing
    if (state.tryEat(Operator::SingleArrow)) /* parse return type */;

Parsing multiple expected elements in a row
-------------------------------------------

``parseOne`` and ``parseAll`` functions allow for parsing of expected elements/values. They generally handle unexpected tokens by adding relevant errors.

``void parseOne(tpc::ParserState&, <Keyword|Special|Operator> value)`` skips if the current token is of the given value. Otherwise adds an error.

``void parseOne(tpc::ParserState&, tpc::Identifier* result)`` parses an identifier into the result if possible. Otherwise adds an error.

``void parseOne(tpc::ParserState&, tpc::OptionalIdentifier* result)`` parses an identifier into the result if possible.

``void parseOne(tpc::ParserState&, tpc::ParserRef<T>* result)`` parses an object of type `T` into the result. Uses the ``parserRef<Element> T::parse(tpc::ParserState&)`` static method that should be implemented by parsable `Element` type objects.

``void parseAll(tpc::ParserState&, ...)`` parses all of the input arguments using ``parseOne`` from left to right for example we can use:

.. code-block:: cpp

    tpc::ParserState& state;
    parseAll(state, Keyword::Fun, /* Identifier* object */, /* list of arguments element parserRef */);

to parse the beginning of a function.

TokenStream
^^^^^^^^^^^

This class implements lower level interactions with a single stream of tokens.

It keeps an iterator on the current position in the stream and guarantees that it's always valid or the end of the stream. Most access methods of the ``TokenStream`` take position relative to the current position as last argument(``usize fwd = 0``).

Token access and movement
-------------------------

``peek`` method allows to access the underlying token relative to the current position.

.. code-block:: cpp

	const Token& peek(usize fwd = 0) const;

``skip`` method moves the iterator forward by the indicated amount.

.. code-block:: cpp

	void skip(usize n = 1);

``next`` method moves the iterator forward by one and returns the token from the previous position. It 

.. code-block:: cpp

	const Token& next();

``size`` method returns the amount of tokens left before the end of the stream including the current token.

.. code-block:: cpp

    usize size() const;

Recursive access
----------------

``isRecursive`` method provides information if a token is recursive.

.. code-block:: cpp

	bool isRecursive(usize fwd = 0) const;

``getRecursive`` method returns a new ``TokenStream`` that is iterating over the recursive sub-tokens of the current token.

.. code-block:: cpp

	TokenStream getRecursive() const;

Token queries
-------------

``bool is<Special|Keyword|Operator|BracketGroup>(usize fwd = 0)`` methods provide a way to check if a particular token is of a particular ``lexer::Token::Type``.

``asKeyword`` and ``asSpecial`` methods interpret underlying value of a token as ``duckling_def::Keyword`` or ``duckling_def::Special``.

.. code-block::cpp

	Keyword asKeyword(usize fwd = 0) const;
	Special asSpecial(usize fwd = 0) const;

``is``, ``isOperator(base::StrId, usize fwd)`` and ``isBracketGroup(lexer::Token::BracketType, usize fwd)`` methods provide ways to compare a particular token with a value or type

.. code-block::cpp

	bool isOperator(base::StrId oper, usize fwd = 0) const;
	bool isBracketGroup(Token::BracketType type, usize fwd = 0) const;
	bool is(<lexer::Token::Type|duckling_def::Keyword|duckling_def::Special|duckling_def::Operator> t, usize fwd = 0) const;

Element
^^^^^^^

``tpc::Element`` is a base class intended to be a building block for AST nodes.

The destructor for derived classes has to be specified.

Important methods requiring implementation
------------------------------------------

``Element::parse`` static method is supposed to be the implementation of parsing a given object. It returns an empty reference only on error and stores the error in ``state``.

.. code-block::cpp

    static ParserRef<Element> parse(ParserState& state);

``trailingSemicolon`` method provides information whether this kind of element should end in a semicolon.

.. code-block::cpp

    bool trailingSemicolon();

``dprint`` method prints elements recursively in a format similar to JSON to specified output stream.

.. code-block::cpp

    void dprint(std::ostream &out) const;

Null aware debug print
----------------------

``nullAwareDprint`` is a helper function that uses ``T::dprint`` implementation to print a ``ParserRef<T>&`` object but handles null case correctly.

.. code-block::cpp

    template<class T>
    void nullAwareDprint(const ParserRef<T> &ref, std::ostream &out)

Identifiers
^^^^^^^^^^^

``Identifier`` and ``OptionalIdentifier`` structures provide easy to access ways of storing ``Element`` identifiers.