\page init-module Init Module

Simple utility module for scheduling functions to execute right after main starts, and right before main finishes,
as well as to run code before main.

It intended use is to run initialization and deinitializaiotn
functions in controlled manner, that for any reason can't or
are inconvenient to be handled automatically by static objects.
