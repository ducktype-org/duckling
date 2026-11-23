@page clah-module Clah

Command-Line Argument Handler is our library for parsing user 
input passed through command line arguments and executing actions 
based on the passed parameters.

Parameters can be passed like in GNU tools:

- Long names: ``--times 3``, ``--times=3``.
- Short names: ``-n 3``, ``-n=3`` or ``-n3``. A short value parameter takes the rest of its
  word as the value, so ``duckc -O3`` is the same as ``duckc -O 3``.
- Short flags can be grouped: ``-abc`` sets the flags ``a``, ``b`` and ``c``. A value parameter
  ends the group, so ``-vO2`` sets the flag ``v`` and gives ``O`` the value ``2``.

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
