\page common-readme Common modules

## clap

\subpage clap-module

  Library for parsing user input passed through command line
  arguments.

## diagnostic

\subpage diagnostic-module

  The Diagnostic module defines a framework for reporting errors,
  warnings, and other messages to the user.

## filesystem

\subpage filesystem-module

  Module implementing `std::filesystem::path` wrapper and reading of files content, with
  automatic memory management.

## init

\subpage init-module

  Simple utility module for scheduling functions to execute right after main starts, and right before main finishes,
as well as to run code before main.

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

## system_command

\subpage system-command-module

  Module implementing system command execution.

## tester

\subpage tester-module

  Implements simple testing framework.

## tokenizer

\subpage tokenizer-module

  Module implementing lexing of any text according to rules defined by Duckling Programming Language,
  keywords, specials and operators definitions provided by `lang_definition` module.