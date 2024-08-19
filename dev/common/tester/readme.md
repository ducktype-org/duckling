\page tester-module Tester Module

# Tester

[Description](#Description)  
[Interface](#Interface)  
[Usage](#Usage)  
[Files](#File-list)  
[Notes](#Notes)

# Description

This module provides simple framework for writing tests. Has tools to help group tests together and print their results. Additionally provides unified interface for assertions and similar mechanisms.

# Interface

## Files:

* [tester.hpp](tester.hpp) - all functionalities
* [tester.cpp](tester.cpp) - implementation

## Symbols:

All symbols are in namespace `tester` and come from [tester.hpp](tester.hpp).

### TestSuite

Class that is intended to be inherited from while writing test classes.

* `assertTrue(bool v, std::string_view err, bool critical = true)`
  If `v` is `false` then: adds `err` to test output, marks test as failed.
  Additionally if `critical` is `true` and `v` is `false` execution of a test will stop.

* `void fail(std::string_view err);`
  Stops execution of tests and add `err` messuage to output. Marks test as failed.

* `void message(std::string_view mess);`
  Adds `mess` message to tests output.

### TESTER_ADD_TEST

Macro used to add tests inside test class constructor

# Usage

[example.cpp](misc/example.cpp).

# Notes
