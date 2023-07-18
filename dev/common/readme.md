# Module overview:

## config
  Module implementing parsing command line arguments.
  It also provides interface for easy creation of parsing rules,
  has functions dedicated for parsing arguments of main rift compiler and test options.

## filesystem
  Module implementing `std::filesystem::path` wrapper and reading of files content, with
  automatic memory management.
  
## lexer
  Module implementing lexing of any text according to rules defined by Rift Programming Language,
  keywords, specials and operators definitions provided by `rift_definition` module.

## printer
  Module implementing printing to console with logical structure of messages, message packs, etc.
  It also provides additional features like message levels, and colors.

## rift_definitions
  Defines keywords, specials and operators used by lexer.
  Provides interface for switching between definitions.

## tester
  Implements simple testing framework.

