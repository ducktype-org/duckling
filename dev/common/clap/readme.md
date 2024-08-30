@page clap-module Clap

Command-Line Argument Parser is our library for parsing user input passed through command line arguments.

A simple ``cat`` using clap:

@include clap_example_cat.cpp

Help message illustration:

```
❯ ./clap_example_cat -h
    Usage: clap_example_cat <file> [options] [another_file...]
    Options:
      -h, --help               Display this information.
      -n, --times <int>        How many times to print each content
```