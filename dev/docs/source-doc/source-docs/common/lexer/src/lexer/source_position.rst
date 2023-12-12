==============
SourcePosition
==============

This is detailed code documentation for
`source_position.hpp <https://github.com/rift-lang/rift-dev/blob/main/dev/common/lexer/src/lexer/source_position.hpp>`_.

SourcePosition is used for storing a position of the token in a source file.

.. code-block:: cpp

    ...
    // Suppose we have a lexer::Token token
    SourcePosition position = token.getPosition();

    // now we can retrieve source content from it
    std::string source_code = position.getSourceChars();
    std::cout << "Source code: " << source_code << '\n';

    // or generate an error regarding this piece of source code
    std::string error_message = position.genErrorMsg("You wrote a bad code here!");
    std::cout << error_message << '\n';
    ...

.. doxygenclass:: lexer::SourcePosition
   :members:
