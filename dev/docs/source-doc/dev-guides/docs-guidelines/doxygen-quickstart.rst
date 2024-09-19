==================
Doxygen Quickstart
==================

.. contents::
    :depth: 2
    :local:

There are two distinct places where Doxygen documentation can be written:

* in the :ref:`source code files <source-files>`
* in the :ref:`Markdown files <md-files>`

.. _source-files:

Source Code Files
=================

Doxygen documentation can be written directly in the source code files.
This is useful when you want to provide a detailed description of a class, function, or variable.

Start each file with a comment block:

.. code-block:: cpp

    /**
     @file foo.hpp
     @brief Brief description of the file.
     @author John Doe
     @date 2021-10-01
     
     Detailed description of the file.
     */

Note the double asterisk ``**`` starting the comment block. This is important, as Doxygen uses it to identify the beginning of the comment.
After the ``/**`` each line should either start with space (as in the example above) or with an asterisk ``*``, like this:

.. code-block:: cpp

    /**
     * @file foo.hpp
     * @brief Brief description of the file.
     * @author John Doe
     * @date 2021-10-01
     *
     * Detailed description of the file.
     */

.. note::
    Doxygen commands can start with ``@`` or ``\``, but the latter is discouraged.

You can also add Doxygen commands to describe classes, functions, and variables:

.. code-block:: cpp

    /**
     * @class Foo
     * @brief Brief description of the class.
     * @details Detailed description of the class.
     */
    class Foo {
    public:
        /**
         * @brief Brief description of the function.
         * @details Detailed description of the function.
         * @param x Description of the parameter.
         * @return Description of the return value.
         */
        int bar(int x);
    };

    /**
     * @brief Brief description of the variable.
     * @details Detailed description of the variable.
     */
    int baz;

For more information on Doxygen commands, see the `official Doxygen documentation <https://www.doxygen.nl/manual/commands.html>`_.

.. _md-files:

Markdown Files
==============

Markdown files can also be used to generate Doxygen documentation. 
This is useful when you want to provide a high-level overview of the codebase, 
or when you want to provide a tutorial or a guide.

To generate Doxygen documentation from Markdown files, you need to add the following line at the beginning of the file:

.. code-block:: markdown

    @page <page_reference_name> <page_title>

This will mark the file as a *Doxygen page*.

You can then reference this page from anywhere in the codebase by using the ``@ref`` command, see below.

.. code-block:: markdown

    @ref <page_reference_name>

Linking subpage to its parent
-----------------------------

To link a subpage to its parent page, you can use the following command anywhere in the parent page:

.. code-block:: markdown

    @subpage <subpage_reference_name>

This will create a link to the subpage.

Linking other Doxygen entities like pages, files, functions, classes, etc. can be done using the ``@ref`` command.

For example, to link to a file, you can use:

.. code-block:: markdown

    @ref <file_name>

It integrates with the Markdown syntax, so you can use it in the same way you would use a hyperlink:

.. code-block:: markdown

    [Link text](@ref <file_name>)

There are also alternative syntaxes like ``[Link text](#<file_name>)``.

Doxygen autolink
================

Doxygen can automatically detect if a word is a Doxygen entity and create a link to it.
This works for file names, functions, classes and many others. 
Full list can be found `here <https://www.doxygen.nl/manual/autolink.html>`_.

.. _doxygen-examples:

How to add code examples
========================

In external file
----------------

To add an example to a Doxygen page, you should create ``examples`` folder and place the example file there.
Then you can mark it as an example using the following command:

.. code-block:: markdown

    @example <example_file_name>

If there are multiple files with the same name in different directories, you can specify parf of the path to the file:

.. code-block:: markdown

    @example <part/of/the/path/to/file.cpp>

The command can be in any source file (not in the markdown file), it doesn't matter where. 

.. warning::
    Anything after the :code:`@example <file_name>` command is treated 
    as an **example description**, 
    so if you write a description of the file, put it at the end.

    .. code-block:: cpp

        /** 
         @file foo.hpp
         
         Description of the file...
         
         @example example.cpp
         This is an example of how to use the function `foo`.

         This is also a description of the "example.cpp", not the "foo.hpp".
         */
    
.. note::
    The example command doesn't create clickable link to the example file from that place.
    It only indicates to Doxygen, that the file is an example, so the Doxygen will create links 
    from every function, class, etc. that is used in the example to that example.

    So if you want to see if the example is correctly linked, you have to see the documentation 
    of some function or class that is used in the example. There should be something like:

    .. code-block:: markdown

        function_name()
        This is the description of the function.

        Examples:
        example_file_name.cpp.

    There is also "examples" page in the Doxygen documentation, where all examples are listed.


Creating a link to example file from the documentation is currently not possible, see :ref:`excluded_directories`.

You can include the contents of the example (or any file) in the documentation using the ``@include`` command.

.. code-block:: markdown

    @include <file_name>

.. tip::
    Often you will want to use the file as an example as well as include it in the documentation.
    In this case, you can use both commands:

    .. code-block:: markdown

        @include <example_file_name>
        @example <example_file_name>


In a code snippet
-----------------

To include an example in a code snippet, you can use the following command:

.. code-block:: cpp

    /**
    * @code
    * ... 
    * @endcode
    */

This will include the code between ``@code`` and ``@endcode`` in the documentation.

.. note::
    Everything in the Doxygen description is also interpreted as Markdown, so you can use headers, lists, tables,
    as well as code snippets:

    .. code-block:: cpp

        /**
         ```cpp
         int main() {
            return 0;
         }
         ```
         */

Useful commands
===============

All Markdown syntax is available in Doxygen documentation, so you can use headers, lists, tables, etc.
However, there are some extensions that can be useful. All list can be found `here <https://www.doxygen.nl/manual/markdown.html>`_.

Table of contents
-----------------

To create a table of contents, you can use the following command:

.. code-block:: markdown

    @tableofcontents
    or
    [TOC]

Notes and warnings
------------------

To create a note, warning, attention and remark (hint, tip) you can use the following command:

.. code-block:: markdown

    @note This is a note.
    @warning This is a warning.
    @attention This is an attention.
    @remark This is a remark.
    @tip This is also a remark.
    @hint This is also a remark.

You can also use Github-style notes (``>`` are important):

.. code-block:: markdown

    > [!NOTE]
    > This is a note.
    > 
    > [!WARNING]
    > This is a warning.
    > 
    > [!TIP]
    > This is a tip.
    > 
    > [!IMPORTANT]
    > This is important.

Links to section headers
------------------------

To create a link to a section header, you have to create label first:

.. code-block:: markdown

    # My section header {#label_name}

Then you can link to it using ``@ref`` command or hash ``#`` syntax:

.. code-block:: markdown

    @ref label_name
    or
    [Link text](#label_name)

Images
------

You include image in the documentation the same way you would in Markdown:

.. code-block:: markdown

    ![Example output](common/printer/examples/exampleoutput.png)

Path to the image is relative to the location of dev directory.


.. _excluded_directories:

Directories excluded from Doxygen
=================================

Currently, the following directories are excluded from Doxygen documentation generation, 
but can be included or marked as examples:

* ``*/examples/*``
* ``*/tests/*``

This is done to avoid generating documentation for classes and functions in examples and tests.

.. code-block:: cpp

    struct A {
        int b;
        void test_something();
    };
    // We don't want to generate documentation for this class and function.

.. note::
    If Doxygen links capital letter "A" to the class "A", you can write it as ``%A``.
