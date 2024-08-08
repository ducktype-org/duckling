========================
Documentation guidelines
========================

.. @TODO:
.. Write more tutorials

.. contents::
    :depth: 2
    :local:

Documentation structure
=======================

Duckling docuemntation is divided into two parts:

* ``duckling-doc`` - user documentation, describing language funcionalities, usage, assumptions and goals 
* ``source-doc`` - developer documentation, describing implementation details and providing resources for developers

``source-doc`` is further divided into:

* ``dev-guides`` - guidelines, tutorial and instructions
* ``dev-handbook`` - high level description of source code
* ``source-docs`` - low level description of source code, implementation details and libraries usage

Basic rst
=========

Documentation is written primarly in ``reStructuredText``. Idea behind ``reStructuredText`` (``.rst`` files) is similar to ``Markdown``. Sorce files are supposed to be easily redable and editable for humans, while at the same time providing funcionalities for generating nice-looking HTML documents.

Headers
-------

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
-----

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
-----------------

* ``code`` - use double backticks: \`\`code\`\`
* *italic* - use single asterisk: \*italic\*
* **bold** - use double asterisk: \*\*bold\*\*

You can also use "\\" sign to escape characters.

Code blocks
-----------

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

Doxygen and breathe
===================

``Doxygen`` is a tool to generate documentation for C++ code and ``breathe`` enables doxygen usage in sphinx. To include generated documentation use breathe directives, for example:

.. code-block:: rst

    .. doxygenclass:: base::Exception

See `breathe documentation <https://breathe.readthedocs.io/en/latest/index.html>`_ for full list of directives.

See also
========

* :doc:`Printer examplary documentation </source-doc/source-docs/common/printer/index>`
* `Discord documentation channel <https://discord.com/channels/860531247826731029/1106545291852783627>`_
