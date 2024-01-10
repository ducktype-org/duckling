========
Optional
========

``base::Optional`` is our wrapper around ``std::optional``.

Example
=======

.. code-block:: cpp
    :caption: Simple example

    #include <base/optional.hpp>
    #include <cassert>

    int main() {
		// Create an empty optional
		base::Optional<int> opt;

		ASSERT_EQUAL(false, opt.has_value());
		// or simply
		ASSERT_EQUAL(true, opt.empty());

		opt = 1;
		ASSERT_EQUAL(1, *opt);
		ASSERT_EQUAL(1, opt.value());

		base::Optional<int> opt2(2);
		// Mapping the value, and changing a type!
		ASSERT_EQUAL(2.2, *opt2.map([](int v) { return v * 1.1; }));

        // --------------------------------------------------

        // base::Optional can also hold a reference!
		std::string                  name = "Rift";
		base::Optional<std::string&> opt_name(name);

		opt_name.value().push_back('!');
		ASSERT_EQUAL("Rift!", name);
    }

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
