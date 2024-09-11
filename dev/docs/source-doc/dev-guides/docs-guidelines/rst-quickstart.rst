===========================
ReStructuredText Quickstart
===========================

Documentation is written primarly in ``reStructuredText``. 
Idea behind ``reStructuredText`` (``.rst`` files) is similar to ``Markdown``. 
Source files are supposed to be easily redable and editable for humans, 
while at the same time providing funcionalities for generating nice-looking HTML documents.
Sphinx framework possesses significant extensibility capabilities including 
the ability to hook into almost every point of the build process.

Here's a quick guide to writing ``reStructuredText``. 

Headers
=======

In ``.rst`` headers are created by underlining text with some symbols. While technicly order of nesting doesn't matter we use following standard:

.. code-block:: rst

    =====
    Title
    =====

    Section
    =======

    Subsection
    ----------

    Subsubsection
    ^^^^^^^^^^^^^

    Paragraphs
    """"""""""

Lists
=====

To create ordered lists use ``#.`` sybmols and to create unordered lists use ``*`` symbol. Of course lists can be nested and you can use other directives inside lists. When nesting something in a list, keep in mind that it should be indented to flush with the parent list item text (ie. three spaces in ordered lists and two spaces in unordered lists).

.. code-block:: rst

    Ordered list:

    #. First item

    #. Second item

       #. Second item, first subitem

       #. Second item, second subitem
       
    #. Third item

    Unordered list:

    * Item

      * Subitem

      * Another subitem

    * Another item

    * Yet another item

    Mixed list:

    * Item

      #. First subitem

      #. Second subitem

    * Another item

Inline formatting
=================

* ``code`` - use double backticks: \`\`code\`\`
* *italic* - use single asterisk: \*italic\*
* **bold** - use double asterisk: \*\*bold\*\*

You can also use "\\" sign to escape characters.

Code blocks
===========

Use ``code-block`` directive to write a block of code. You can also specify a language to set highlighting.

.. code-block:: rst
    
    .. code-block:: cpp

        int main() {
            return 0;
        }


Tables of contents
==================

In ``rst`` you can easily generate both internal and external tables of contents.

To create internal table of contents use ``contents`` directive.

.. code-block:: rst

    .. contents::
        :depth: 2
        :local:

To create external table of contents use ``toctree`` directive.

* Use ``maxdepth`` to set depth of generated sub items. 
* Use ``caption`` to set the caption.
* Use ``glob`` to enable auto adding items that match pattern.

.. code-block:: rst

    .. toctree::
        :maxdepth: 1
        :caption: Contents:
        :glob:

        directory/file.rst
        folder/doc.rst
        *

Cross-references
================

You can easily create cross-references to other parts of the documentation.

General syntax is: ``:label:`target` ``. 

You may supply an explicit title and reference target, 
like in reStructuredText direct hyperlinks: 
:role:`title <target>` will refer to target, but the link text will be title.

Cross-referencing documents
---------------------------

To cross-reference documents use ``:doc:`` role.

For example to reference document located in ``path/to/document.rst`` use:

.. code-block:: rst

    :doc:`path/to/document`
    :doc:`Title of the link <path/to/document>`

Cross-referencing sections
--------------------------

To cross-reference sections use ``:ref:`` role.
But in the first place you need to create a label for the section.

To create a label use ``.. _label:`` directive.

.. code-block:: rst

    .. _label:

    Section
    =======

    Some text

To reference the section use:

.. code-block:: rst

    :ref:`label`
    :ref:`Title of the link <label>`

You can also reference other elements like figures, tables, downloadable documents, etc.
To reference anything use ``:any:`` role. First, it tries standard cross-reference 
targets that would be referenced by doc, ref or option. If it fails, it tries to
find any target with the given name.


