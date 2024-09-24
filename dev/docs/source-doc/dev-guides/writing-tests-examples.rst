==========================
Writing tests and examples
==========================

Writing tests
=============

When writing tests, you should use our custom test framework.
The most up-to-date documentation can be found in the Doxygen documentation.
Our test framework is located in the `tester` module, in the `common` directory.

.. note::

    To easily open the Doxygen documentation, you can use the following command inside the build folder:

    .. code-block:: bash

        ninja open-doxygen-docs

    There you can use the search bar to find the ``tester`` module.


.. tip::

    The easiest way to learn how to write tests is to look at the existing ones.


The first step is to create a ``tests`` directory in your module (if it doesn't exist yet).
Then you should create a test file with your test using our custom test framework.

After you created test file with our ``TESTER_COMMON_MAIN`` macro, 
you have to add it to the CMake target list.
You can do this by using our custom CMake function:

.. code-block:: cmake
    :caption: CMake file of the module

    rift_add_test(test_pack test_name source USES module1 module2 ...)
    # example:
    rift_add_test(common filesystem_test_fstree tests/fs_tree_test.cpp 
        USES Filesystem)

After you added the test to the CMake file, you can compile it by using one of the following commands:

.. code-block:: bash

    ninja <test_name>
    # or
    ninja build_<test_pack>_tests
    # or
    ninja build_all_tests

To run the test, you can just run the compiled binary:

.. code-block:: bash

    ./bin/<test_name>

.. seealso::

    On other ways to compile and run the tests, see the :ref:`running-the-tests` section.

Writing examples
================

When writing examples, you should place them in the `examples` directory in the module you are working on.
If the example is small and doesn't require a separate file, you can place it in the source file in the Doxygen comment
(that documents the file, function, or class).

Each example file should contain ``main`` function and should be able to compile and run.
You can add the example to the CMake target list by using our custom CMake function:

.. code-block:: cmake
    :caption: CMake file of the module

    rift_add_example(example_name source USES module1 module2 ...)
    # example:
    add_example(filesystem_temp_path_example examples/temp_path_creation.cpp
	USES Filesystem)

There are cases when you don't want to compile the example (e.g. you have some pseudocode in it),
then you shouldn't add it to the CMake.

After you added the example to the CMake file, you can compile it by using one of the command:

.. code-block:: bash

    ninja <example_name>

To run the example, you can just run the compiled binary :code:`./bin/<example_name>`.

Adding examples to the Doxygen documentation
--------------------------------------------

To see how to link or write examples in the Doxygen documentation, 
see the :ref:`doxygen-examples` section.
