====================
Building the project
====================

.. contents::
    :depth: 2
    :local:

Here you can find instructions on how to build the project.
Most of the time you will want to use our tailored build script ``toolbox.py``, 
that will take care of all the details for you.

After you have cloned the repository you should navigate to the ``dev`` directory.
All commands from now on should be run from this directory.

How to use toolbox
==================

To run the toolbox, you need to have Python3 installed on your system, as well as Python package manager ``pip``.
It requires python `Click <https://click.palletsprojects.com/en/8.1.x/>`_ package to be installed.

.. code-block:: bash

	pip install click

To run toolbox you can use ``python3 toolbox.py`` command or ``./toolbox.py``.
To see all available arguments run the script without any options:

.. code-block:: bash

	./toolbox.py


Standard build
==============

Thirst thing you need to do is to initialize the repository by running:

.. code-block:: bash

	./toolbox.py init

This will initialize the git submodules, create python virtual environment 
for documentation (you can do it seperately as well) 
and download binaries of the required tools (clang-format, ccache).

.. important::

	If you are starting the development and the LLVM backend is not installed on your system, you can download it by running:

	.. code-block:: bash

		./toolbox.py download-llvm

	You should also install **the required dependencies** for the project.
	The full list of dependencies is available 
	in the main `readme file of the project github repository <https://github.com/ducktype-org/rift-dev/tree/main>`_. 

To create build directory run:

.. code-block:: bash

	./toolbox.py build

You will be asked a series of questions about the build configuration.
Inside square brackets you can see the default value that will be used if you just press enter.

.. code-block:: bash

	> ./toolbox.py setup-build
	:caption: Example output of the build script
	Build dir name [build]: 
	Build system (Ninja, Unix Makefiles) [Ninja]: Ninja
	build type (Debug, Release, RelWithDebInfo, MinSizeRel) [debug]: 
	Build docs [Y/n]: 
	Compiler path [g++]: 
	Use ccache [y/N]: 
	Enable coverage [y/N]: 
	[INFO]: Setting up a build folder...
	...

It is recommmended to use Ninja build system, as it by default uses all available cores to compile the project.
Ninja works the same way as Unix Makefiles.
To install Ninja on Ubuntu run:

.. code-block:: bash

	sudo apt install ninja-build

After initialization, you can compile the project by running:

.. code-block:: bash

	cd build
	ninja <target>
	ninja all

The compiles binaries are inside the ``build/bin`` directory.


Compiling with test coverage enabled
====================================

To compile with test coverage enable you should use the toolbox script to create the build directory first.

.. code-block:: bash

	./toolbox.py setup-build

When asked about enabling coverage, type ``y``.

.. code-block:: bash
	
	> ./toolbox.py setup-build
	...
	Enable coverage [y/N]: y
	...

.. tip::

	You can also achieve the same effect by running:

	.. code-block:: bash

		./toolbox.py setup-build --coverage


	Providing answer to the questions and running the command with the appropriate flags have the same effect, 
	thanks to the Click library.

To run the tests with coverage, you can use the following command:

.. code-block:: bash

	./toolbox.py coverage

Compiling with CCACHE enabled
=============================

To compile with CCACHE enabled you should use the toolbox script.

.. code-block:: bash

	./toolbox.py setup-build --ccache


.. _running-the-tests:

Running the tests
=================

To compile and run the tests you can use the following command:

.. code-block:: bash

	./toolbox.py test

This will compile the project and run the tests. We use the CTest tool 
from CMake to manage the test files. There is a CMake command to compile 
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

	You can also use the ``ctest`` command to run the tests, for example with rexeg name filter:

	.. code-block:: bash

		ctest -R vm_

	This will run all tests that have ``vm_`` in their name at the beggining.

Compiling the documentation
===========================

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
