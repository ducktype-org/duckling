@page hashing-module Hash Module

This module implements hashing utilities that allow to easily add hashing support to any type and
to hash any set of objects using a hashing algorithm of choice. Module provides some ready
algorithms and allows to add more.


General overview
================

There are three parties involved in hashing process:
* writers of classes that need to be hashed
* ones who need to hash some objects
* writers of hashing algorithms

This module provides tools for easy and composable integration of those processes.

Hooking up hashing for a class
==============================

This is the most common use case and also the simplest one. There are three ways for enabling a hashing
support for a class:

`hashDecompose()`
-----------------

The quickest way is to define a friend function in your class with this signature:

~~~~~cpp
    friend constexpr auto hashDecompose(const T& t) noexcept { /*...*/ }
~~~~~

Inside the function all you have to do is to tell which bases/fields are part of the `observable state` of your class.
For example, `string` type should pass its `data` and `size` but not `capacity` as it is not visible in the comparisons.

To list all subfields simply return `std::tie` of all of them in order in which you would like them to be added to hash.
To pass bases you can use `getBase<Base>(t)` which additionally checks if you are actually casting to the base class.
Also a variadic version `getBases<Bases...>(t)` is available. It returns a object that can be decomposed by `addToHash()`
so the hash value may differ if swapped to this utility.

~~~~~cpp
    class C : public Base1, public Base2 {
        int a, b;
        std::string s;
        friend auto hashDecompose(const C& c) {
            using namespace hashing;
            return std::tie(getBase<Base1>(c), getBase<Base2>(c), c.a, c.b, c.s);
            // or
            return std::tie(getBases<Base1, Base2>(c), c.a, c.b, c.s);
        }
    }
~~~~~


`addToHash()`
-------------

~~~~~cpp
    friend constexpr auto addToHash(hashing_algorithm auto& h, const T& t) noexcept { /*...*/ }
~~~~~

As you can see, first parameter is constrained by a concept which you can get by including `<hashing/hash_algorithm_utils.hpp>`.
Though it is not strictly necessary it can help to detece bugs early and gives somehow better error messages.


Adding type to `addToHash()` template fallback
----------------------------------------------

abc

