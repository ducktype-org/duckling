\page common-readme Common modules

# Module overview:

This module contains useful utilities and tools that are used in other parts of the project.

## clap

\subpage clap-module

  Library for parsing user input passed through command line
  arguments.

## diagnostic

\subpage diagnostic-module

  The Diagnostic module defines a framework for reporting errors,
  warnings, and other messages to the user.

## config

  Module implementing parsing command line arguments.
  It also provides interface for easy creation of parsing rules,
  has functions dedicated for parsing arguments of main Duckling compiler and test options.

## filesystem

\subpage filesystem-module

  Module implementing `std::filesystem::path` wrapper and reading of files content, with
  automatic memory management.

## json

\subpage json-module

Json module

## listener

\subpage listener-module

## printer

\subpage printer-module

  Module implementing printing to console with logical structure of messages, message packs, etc.
  It also provides additional features like message levels, and colors.

## query_framework

\subpage query-framework-module

  This module provides implementation of Query Framework used in compiler.

## tester

\subpage tester-module

  Implements simple testing framework.

## tokenizer

\subpage tokenizer-module

  Module implementing lexing of any text according to rules defined by Duckling Programming Language,
  keywords, specials and operators definitions provided by `lang_definition` module.

## hashing

\subpage hashing-module

  This module implements hashing utilities that allow to easily add hashing support to any type and
  to hash any set of objects using a hashing algorithm of choice. Module provides some ready
  algorithms and allows to add more.
