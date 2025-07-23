@page clah-module Clah

Command-Line Argument Handler is our library for parsing user 
input passed through command line arguments and executing actions 
based on the passed parameters.

A simple ``cat`` using clah:

@include clah_example_cat.cpp

Help message illustration:

```
❯ ./clah_example_cat -h
    Usage: clah_example_cat <file> [options] [another_file...]
    Options:
      -h, --help               Display this information.
      -n, --times <int>        How many times to print each content
```