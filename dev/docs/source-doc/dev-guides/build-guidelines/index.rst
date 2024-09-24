====================
Building the project
====================

.. contents::
    :depth: 2
    :local:


Here you can find instructions on how to build the project.


Prerequisites
=============

To build the project, make sure you have the required dependencies installed.
You can find the list of dependencies in the 
`readme file of the project github repository <https://github.com/ducktype-org/rift-dev/tree/main?tab=readme-ov-file#installing-dependencies>`_.

How to use toolbox
==================
Most of the time you will want to use our tailored build script ``toolbox.py``, 
that will take care of all the details for you.

To run toolbox you can use ``python3 toolbox.py`` command or ``./toolbox.py``.
To see all available arguments run the script without any options:

.. code-block:: bash

	./toolbox.py


Standard build
==============

To build the project, you should use `toolbox.py` that will take care of all the details for you.
The script provides the commands for:

* initializing the project (updating submodules, downloading binaries, etc.),
* configuring Python virtual environment,
* creating the build directory,
* building the documentation,
* compiling and running tests

Details on how to use the script to build the project are 
provided in the main `README.md <https://github.com/ducktype-org/rift-dev>`_ file.

It is recommmended to use Ninja build system, as it by default uses all available cores to compile the project.
Ninja works the same way as Unix Makefiles, so any command, like :code:`ninja <target>` can be replaced with :code:`make <target>`.

After initialization, you can compile the project by running in the build directory:

.. code-block:: bash

	ninja <target>
	ninja all

The compiles binaries are inside the ``build/bin`` directory.

Running the tests
=================

To compile and run the tests you can use the ``test`` command from the ``toolbox``.
This will compile the project and run the tests. 

It's useful to be aware of more direct methods for running the tests.
We use the CTest tool from CMake to manage the test files. 
There is also a CMake command to compile 
and run tests:

.. code-block:: bash

	ninja test # to compile and run
	ninja build_all_tests # to compile all tests
	ninja build_<test_suite>_tests # to compile a specific test suite
	ninja <test_file_name> # to compile a specific test file

You can compile a specific test. For example, if you want to run the ``lexer_test_simple`` test, 
you can run the following commands:

.. code-block:: bash

	ninja lexer_test_simple
	./bin/lexer_test_simple

.. note::

	You can also use the ``ctest`` command to run the tests, for example with regex name filter:

	.. code-block:: bash

		ctest -R vm_

	This will run all tests that have ``vm_`` in their name at the beggining.

Compiling the documentation
===========================

Before compiling the documentation, make sure that you have the Python virtual environment set up (.
If you haven't done it yet, you can do it by running :code:`./toolbox.py setup-venv`. The environment 
should be active when you create the build directory with :code:`./toolbox.py setup-build`.

To compile the documentation (source-doc and doxygen) you can use the following command:

.. code-block:: bash

	./toolbox.py docs

This will compile the documentation and open it in your default browser.

You can also run the CMake target by yourself:

.. code-block:: bash

	ninja docs

This will compile the documentation and put it in the ``build/docs`` directory.
There are also custom targets for opening the documentation in the browser:

.. code-block:: bash

	ninja open-sphinx-docs
	ninja open-doxygen-docs
