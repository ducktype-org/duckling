# Module name (example: my_module)

[Description](#source-doc/dev-guides/old-guidelines/DocumentationTemplate:Description)  
[Interface](#source-doc/dev-guides/old-guidelines/DocumentationTemplate:Interface)  
[Usage](#source-doc/dev-guides/old-guidelines/DocumentationTemplate:Usage)  
[Files](#source-doc/dev-guides/old-guidelines/DocumentationTemplate:File-list)  
[Implementation details](#source-doc/dev-guides/old-guidelines/DocumentationTemplate:Implementation)  

# Description

Module description. Few sentences about what it does and why.

Example:

This module provides functionality for performing binary arithmetic operations on integers of any width modulo $10^9+7$.

# Interface

How to use module. List of functions, types, ... that are exported by module.
It should also state following if necessary: 
List of headers that should be included depending on what functionality is being used.

Example:

Files:

* [my_module.hpp](my_module.hpp) - all functionalities

Symbols:

[my_module.hpp](my_module.hpp). Value of modulus used in operations.  
~~~~~cpp 
constexrp int128_t mod_val = 1e9 + 7
~~~~~

[my_module.hpp](my_module.hpp). Adds two numbers together.
~~~~~cpp
template<class T>
T mod_add(T a, T b)
~~~~~

[my_module.hpp](my_module.hpp). Multiply two numbers together.
~~~~~cpp
template<class T>
T mod_mul(T a, T b)
~~~~~


# Usage

Simple examples of usage, what module should be and shouldn't be used for.
Additional notes on more complex use cases.

Example:

Module should be used only when modular arithmetic is needed.
In other cases use simple `+`, `*` operators.

~~~~~cpp
#include <my_module/my_module.hpp>
#include <string>

int32_t hash(std::string s) {
    int32_t out{};
    for (auto a: s) {
        out = mod_mul(out, mod);
        out = mod_add(out, a);
    }
    return out;
}
~~~~~

# File list

List of source and other files presented in the module.
Every file in the list should be relative link to itself, and have a simple and short description.  

When elements in the list doesn't have any logical order sort them alphabetically.

Example:

* [my_module.hpp](my_module.hpp) - main interface
* [my_module.cpp](my_module.cpp) - main implementation
* [test.cpp](test.cpp) - unit tests

# Implementation

Optional. Top-level description of implementation. 
More detailed description of non trivial tricks, methods and patterns.
