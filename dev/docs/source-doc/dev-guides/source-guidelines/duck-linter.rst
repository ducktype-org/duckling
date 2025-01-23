===========
Duck linter
===========

Duck linter is a simple python program, analyzing source code in order to check for some of 
the code conventions.

It currently performs checks listed below.


Checks
======

Relative import check
---------------------

This check ensures that imports of the form  ``#include "..."`` actually point to the file that is
located in ``source-file-directory /  include-string``.

The compilers usually silently fallback to ``#include <...>`` on failed relative import.
