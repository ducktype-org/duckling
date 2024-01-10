========
Optional
========

``base::Optional`` is our wrapper around ``std::optional``.

Example
=======

.. literalinclude :: optional_simple_example.cpp
    :caption: Simple example
    :language: cpp
    :linenos:

.. code-block:: cpp
    :caption: Macro usage example

    base::Optional<int> opt(4);
    match_optional(opt) {
        opt_some(val) {
            // Opt's value is now accessible through val!
            std::cout << "Value: " << val << '\n';
        }
        opt_none { std::cout << "No value!\n"; }
    }

    // Or simply
    base::Optional<int> magic_number = 42;
    if_opt_some(magic_number, value) { std::cout << "Magic number = " << value << "\n"; }

    if_opt_none(magic_number) { std::cout << "magic_number holds no value.\n"; }

.. code-block::
    :caption: output

    Value: 4
    Magic number = 42

Class details
=============

.. doxygenclass:: base::Optional
   :members:
   :undoc-members:
