===========================
Doxygen Documentation Guide
===========================

.. contents::
    :depth: 2
    :local:

There are two distinct places where Doxygen documentation can be written:
- in the source code files
- in the Markdown (`.md`) files

Markdown Doxygen Pages
----------------------

Markdown files can be used to generate Doxygen documentation. 
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
----------------

Doxygen can automatically detect if a word is a Doxygen entity and create a link to it.
This works for file names, functions, classes and many others. 
Full list can be found `here <https://www.doxygen.nl/manual/autolink.html>`_.

Add example
-----------

In external file
++++++++++++++++

To add an example to a Doxygen page, you should create ``examples`` folder and place the example file there.
Then you can mark it as an example using the following command:

.. code-block:: markdown

    @example <example_file_name>

The command can be in any source file (not in the markdown file), it doesn't matter where. 

.. warning::
    Anything after the command is treated as an **example description**, 
    so if you write a description of the file, put it at the end.

    .. code-block:: cpp

        /** 
         @file foo.hpp
         
         Description...

         @example example.cpp
         This is an example of how to use the function `foo`.

         This is also a description of the "example.cpp", not the "foo.hpp".
         */
    
.. note::
    The example command doesn't create clickable link to the example file from that place.
    It only indicates to Doxygen, that the file is an example.
    To include the example in the documentation, you need to use the ``@include`` command as well.

Creating a link to example file from the documentation is currently not possible, see :ref:`excluded_directories`.

In code snippet
+++++++++++++++

To include an example in a code snippet, you can use the following command:

.. code-block:: markdown

    @include my_favorite_example.cpp

If there are multiple files with the same name in different directories, you can specify parf of the path to the file:

.. code-block:: markdown

    @include base/exceptions.hpp

This will include the file ``exceptions.hpp`` from the ``base`` directory.


Useful commands
---------------

All Markdown syntax is available in Doxygen documentation, so you can use headers, lists, tables, etc.
However, there are some extensions that can be useful. All list can be found `here <https://www.doxygen.nl/manual/markdown.html>`_.

Table of contents
+++++++++++++++++

To create a table of contents, you can use the following command:

.. code-block:: markdown

    @tableofcontents
    or
    [TOC]

Notes and warnings
++++++++++++++++++

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
+++++++++++++++++++++++++

To create a link to a section header, you have to create label first:

.. code-block:: markdown

    # My section header {#label_name}

Then you can link to it using ``@ref`` command or hash ``#`` syntax:

.. code-block:: markdown

    @ref label_name
    or
    [Link text](#label_name)

Images
++++++

You include image in the documentation the same way you would in Markdown:

.. code-block:: markdown

    ![Example output](common/printer/examples/exampleoutput.png)

Path to the image is relative to the location of dev directory.


.. _excluded_directories:
Directories excluded from Doxygen
---------------------------------

Currently the following directories are excluded from Doxygen documentation generation, 
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
