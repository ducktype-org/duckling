====================
Building the project
====================

.. contents::
    :depth: 2
    :local:


Standard build
==============

**The most up-to-date build instructions are in the** `README file in the root of the project <https://github.com/ducktype-org/rift-dev>`_.

The process includes initializing the repository and creating the build directory. 
Remember to install the required dependencies before building the project.

.. tip::
	If you want a more in-depth look at the inner workings of the build system
	you can look at the Python code that makes up the :code:`toolbox.py` script.


Compiling the project
---------------------

Ninja and Unix Makefiles are two alternative build systems that can be used to compile the project.
You can choose which you want to use in the `toolbox.py`.
The default option is the Ninja build system, as it uses all available cores to compile the project by default.
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

The easy way to compile the documentation is to use the ``toolbox.py`` script.
Here's more information on how the script works.

Before compiling the documentation, Python virtual environment from ``requirements.txt``
in ``docs/doc-config`` has to be created. 
The environment should be active when a new build directory with ``CMake`` is created.

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