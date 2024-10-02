====================
Building the project
====================

.. contents::
    :depth: 2
    :local:

Before start
============

Installing dependencies
-----------------------

* **Python3** and **click** library are required for the toolbox script.
* **Doxygen** is used for generating documentation.
* **Graphviz** is dependency used for generating diagrams by the compiler.
* **CMake** and **Ninja** are used for building the project.
* **g++** with version 13 or higher is required for building the project.
* **lcov** is used for generating coverage reports.
* **LLVM** is required for building the project.

Debian/Ubuntu
~~~~~~~~~~~~~

.. code-block:: bash

	sudo apt update -y && \
	sudo apt install python3 python3-click doxygen graphviz-dev cmake ninja-build g++ lcov llvm-dev clang-tidy libzstd-dev zlib1g-dev -y

Arch linux
~~~~~~~~~~

.. code-block:: bash

	sudo pacman -Sy python python-pip python-click doxygen graphviz lcov --noconfirm

.. note::
	
	Some dependencies might be installed by default on your system, but if that's not the case, take a look at the list for Debian/Ubuntu.


Toolbox
=======

The toolbox script is a Python script that helps with the initialization of the repository,
building the project, running tests, and other tasks.
It is located in the ``dev`` directory of the project.

To run the script, when you are in the ``dev`` directory, you can use :code:`./toolbox.py <command>` or :code:`python3 toolbox.py <command>`.

To see the available commands you can run the srcipt without any arguments:

.. code-block:: bash

	./toolbox.py

Similarly, you can get help for a specific command:

.. code-block:: bash

	./toolbox.py <command> --help

Below are the most common commands that you will use when building the project.

.. tip::

	If you want a more in-depth look at the inner workings of the build system
	you can look at the Python code that makes up the :code:`toolbox.py` script.

Setting up the repo
===================

To begin, enter the :code:`dev/` directory and then:

Initialize the repository with toolbox
--------------------------------------

.. code-block:: bash

	./toolbox.py init


This:

* fetches library dependencies
* updates git submodules
* setups virtual environment (important for building docs)
* downloads binaries, etc...

[Optional, but recommended] Installing custom LLVM library
----------------------------------------------------------

.. code-block:: bash
	
	./toolbox.py download-llvm


In case of trouble during build or :code:`setup-build` step, it is advised
to install the latest supported version of LLVM for your system 
(the required dependencies are already listed in the dependencies list above).
But the most reliable way is to download the LLVM locally using toolbox with this command.

After using this command, LLVM is **NOT installed system-wide**, but only for this project.
Using version :code:`18.1.8` should work for most platforms.

The downloaded library is placed in the :code:`scripts/downloads` directory.

Create a build folder
---------------------


.. code-block:: bash
	
	./toolbox.py setup-build


Press "enter" on every prompt to leave default options.


Compiling the project
=====================

Ninja and Unix Makefiles are two alternative build systems that can be used to compile the project.
You can choose which you want to use in the :code:`toolbox.py` when creating a build folder.
The default option is the Ninja build system, as it uses all available cores to compile the project by default.
Ninja works the same way as Unix Makefiles, so any command, like :code:`ninja <target>` can be replaced with :code:`make <target>`.

After initialization, you can compile the project by running in the build directory:

.. code-block:: bash

	ninja <target>
	ninja all

.. important::

	The compiled binaries are inside the ``build/bin`` directory.

Building the docs
=================

If you want to build the docs make sure that in the previous step 
the build directory was created with the :code:`docs` option enabled.
For building the documentation python virtual environment 
created in the :code:`init` step is used.

.. code-block:: bash
	
	./toolbox.py docs


This builds the docs and opens them in your favorite browser. 
Leave defaults if you chose defaults in previous step.

Using CMake targets
-------------------

The more direct way to compile the documentation is to use the CMake targets.
After you have created the build directory,
to compile the documentation (source-doc and doxygen) you can run the build target:

.. code-block:: bash

	ninja docs

This will compile the documentation and put it in the ``build/docs`` subdirectory.
There are also custom targets for opening the documentation in the browser:

.. code-block:: bash

	ninja open-sphinx-docs
	ninja open-doxygen-docs

.. note::

	Sometimes when creating new files in the documentation, Sphinx might not recognize them.
	To fix this, you can run the ``ninja clean`` command or rerun the CMake configuration.


Docs configuration
------------------

Our Sphinx documentation configuration is in the ``docs/doc-config`` directory.
It is currently a separate Github repository that is included as a submodule in the main repository.
When doing changes to the configuration, you should commit them to the submodule repository and then update 
the main repository with the new submodule commit.

Running the tests
=================

To compile and run tests, you can use the toolbox script:

.. code-block:: bash
	
	./toolbox.py test     # regular tests
	./toolbox.py test -m  # run tests under valgrind

It's useful to be aware of more direct methods for running the tests.
We use the CTest tool from CMake to manage the test files. 
There is also a CMake command to compile 
and run tests:

.. code-block:: bash

	ninja test                     # to compile and run
	ninja build_all_tests          # to compile all tests
	ninja build_<test_suite>_tests # to compile a specific test suite
	ninja <test_file_name>         # to compile a specific test file

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


Testing coverage
================

Before running this command make sure you build folder has enabled coverage.

.. code-block:: bash
	
	# This is only needed if build folder was not prepared for coverage.
	./toolbox.py setup-build --coverage

	./toolbox.py coverage
