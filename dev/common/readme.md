\page common-readme Common modules

# Module overview:

## config
  Module implementing parsing command line arguments.
  It also provides interface for easy creation of parsing rules,
  has functions dedicated for parsing arguments of main duckling compiler and test options.

## filesystem

\subpage filesystem-module

  Module implementing `std::filesystem::path` wrapper and reading of files content, with
  automatic memory management.
  
## tokenizer

\subpage tokenizer-module

  Module implementing lexing of any text according to rules defined by Duckling Programming Language,
  keywords, specials and operators definitions provided by `duckling_definition` module.

## printer

\subpage printer-module

  Module implementing printing to console with logical structure of messages, message packs, etc.
  It also provides additional features like message levels, and colors.

## tester

\subpage tester-module

  Implements simple testing framework.

## json

\subpage json-module

Json module

