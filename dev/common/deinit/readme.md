\page deinit-module Deinit Module

Module simple utility for scheduling functions to execute after the program finishes (i.e. in global object destructors).

It is in practice the same functionality as `std::atexit`, however `std::atexit` has some critical drawbacks like, e.g. (from cpp reference):

> The implementation is guaranteed to support the registration of at least 32 functions. The exact limit is implementation-defined. 

