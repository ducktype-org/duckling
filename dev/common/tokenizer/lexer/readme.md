@page lexer-module Lexer

@tableofcontents

Module implementing the conversion from source code to tokens using keyword definitions provided by `lang_definitions` module.

@note Lexer doesn't currently support other varieties of strings (format, raw), number separators, some comment features (token from text, recursive comments) and handling of ignorable format controls. It also doesn't currently do any normalization.

Usage
=====

~~~~~cpp
    :caption: Basic usage

    #include<lexer/lexer.hpp>
    #include<lang_definitions/key_spec_op.hpp>

    int main() {
        lexer::init();
        lang_def::setKeywordMode(lang_def::KeywordMode::DucklingSource);

        fs::FilePath file("path/to/duckling/file");
        lexer::TokenData td = lexer::tokenizeFile(file);
    }
~~~~~

If decoding or tokenization fails then `lexer::tokenizeFile` function prints error to `std::cerr` and throws a `base::LogicError`.

Interface
=========

All symbols are in namespace `lexer`.

The main ways of interfacing with the lexer are in files `lexer.hpp` and `token.hpp`.

lexer.hpp
---------

`lexer.hpp` contains the interface needed to run the lexer.

### init

Initializes resources that are needed to run the lexer.

~~~~~cpp
    void init();
~~~~~

### tokenizeFile

Returns a tokenization of a given file or outputs errors to `std::cerr` and throws `base::LogicError`.

~~~~~cpp
    TokenData tokenizeFile(fs::FilePath file);
~~~~~

token.hpp
---------

`token.hpp` contains the interface with the structures returned by lexing.

### TokenData

The result of lexing. It contains:
 
1. `Tokens tokens` - List of top level tokens.
2. `Token eof_sentinel` - Token that can be used as EOF.
3. `fs::FileContent file_content` - Pointer to the contents of the source file.

### Token

The building block of the result of lexing. It keeps all of the important information about the properties of a given token and has an interface (shown in the source code section) that allows to check against that information.

It includes two enums:

1. `Type` - a general type of a token 
2. `BracketType` - type describing different bracket types.

Tests
=====

- test.cpp - contains tests that check if correct types of tokens are created in the correct layout
- token_position_test.cpp - checks for differences in output with a correct output.
- decode_test.cpp - checks if decode correctly accepts well encoded programs and finds errors in badly encoded programs.
