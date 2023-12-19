===================
Markdown quickstart
===================

For more detailed, in-deph instructions go to `GitHub Docs <https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax>`_.

Headers
=======

To create a section header use hash symbol.

.. code-block:: markdown

    # Section

    Some text

    # Another section

    Some more text

To create diffrent headeing levels use more hashes.

.. code-block:: markdown
    
    # Header level 1

    Some text

    ## Header level 2

    Some more text

    ### Header level 3

    And even more ...

Headers automatically create anchors. You can use them to link different sections of your documents. See :ref:`source-doc/dev-guides/markdown-guidelines:links` to learn more.

Inline formatting
=================

Bold text

    Use double asterisk symbol to show text as **bold**.

    .. code-block:: markdown

        Use double asterisk symbol to show text as **bold**.

Italic text

    Use single underscore symbol to show text as *italic*.

    .. code-block:: markdown

        Use single underscore symbol to show text as _italic_.

Code text

    Use single backquote symbol to show text as ``inline code``.

    .. code-block:: markdown

        Use single backquote symbol to show text as `inline code`.

Code blocks
===========

To show text as a block of code use three backticks or tildes.

    .. code-block::

        {
            "firstName": "John",
            "lastName": "Smith",
            "age": 25
        }
    
    .. code-block:: markdown

        ```
        {
            "firstName": "John",
            "lastName": "Smith",
            "age": 25
        }
        ```

    .. code-block:: markdown

        ~~~
        {
            "firstName": "John",
            "lastName": "Smith",
            "age": 25
        }
        ~~~

You can also add language specific syntax highlighting by specifying language next to the backquotes.

    .. code-block:: json

        {
            "firstName": "John",
            "lastName": "Smith",
            "age": 25
        }
    
    .. code-block:: markdown

        ```json
        {
            "firstName": "John",
            "lastName": "Smith",
            "age": 25
        }
        ```

Links
=====

Simple link

Use single angle brackets to show text as clickable link.

    Go to `<https://www.google.pl/>`_.

    .. code-block:: markdown

        Go to <https://www.google.pl/>.

Text as link

Use square brackets and pharenteses to add link to text.

    Go to `google <https://www.google.pl/>`_.

    .. code-block:: markdown

        Go to [google](https://www.google.pl/).

Link to anchor
    
Use hash symbol to link specific part of your document.

    Go to :ref:`specific section <source-doc/dev-guides/markdown-guidelines:Inline formatting>`.

    .. code-block:: markdown

        Go to [specific section](#inline-formatting).

Lists
=====

Use hyphen, plus sign or asterisk to create unordered lists.

    * first item
    * second item
    * third item
    * another item

    .. code-block:: markdown
    
        - first item
        - secont item
        - third item
        - another item

    .. code-block:: markdown
    
        + first item
        + secont item
        + third item
        + another item

    .. code-block:: markdown
    
        * first item
        * secont item
        * third item
        * another item

Use numbers followed by period to create ordered lists.

    #. first item
    #. second item
    #. third item
    #. another item

    .. code-block:: markdown
    
        1. first item
        2. secont item
        3. third item
        4. another item

Note that even if you use numbers in random order, the output will appear in order. Both examples below yield the same output as the one above.

    .. code-block:: markdown
        
        1. first item
        1. secont item
        1. third item
        1. another item

    .. code-block:: markdown
        
        1. first item
        8. secont item
        3. third item
        5. another item

Spaces, new lines and indentation
=================================

Remember to put new lines after headers and before lists, tables, etc.

    .. code-block:: markdown
        
        # Header
            <!-- Here -->
        Text
            <!-- Here -->
            * First
            * Second

To add new line use double spaces at the end of line or `<br>` anywhere else.

    .. code-block:: markdown

        A  <!-- Double space -->
        B<!-- No Double space -->
        C
        <br> <!-- It will break things like lists -->

Use indentation to the level of parent element (with spaces) or double space if indenting to parent doesn't make sense or break things.
