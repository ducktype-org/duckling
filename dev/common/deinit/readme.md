\page deinit-module Deinit Module

Simple utility module for scheduling functions to execute after the program finishes (i.e. after main).

It is a very similar functionality to `std::atexit`, however `std::atexit` has some critical drawbacks like for example (from cpp reference):

> The implementation is guaranteed to support the registration of at least 32 functions. The exact limit is implementation-defined. 

