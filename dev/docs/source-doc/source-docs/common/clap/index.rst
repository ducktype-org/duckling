====
Clap
====

Command-Line Argument Parser is our library for parsing user input passed through command line arguments.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/clap/index.rst
    tests.rst

Example
=======

.. literalinclude :: clap_example_cat.cpp
    :caption: A simple ``cat`` using clap.
    :language: cpp
    :linenos:

.. code-block::
    :caption: Help message illustration

    ❯ ./clap_example_cat -h
    Usage: clap_example_cat <file> [options] [another_file...]
    Options:
      -h, --help               Display this information.
      -n, --times <int>        How many times to print each content
