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
<!-- Also a variadic version `getBases<Bases...>(t)` is available. It returns a object that can be decomposed by `addToHash()` so the hash value may differ if swapped to this utility. -->
<!-- // or
return std::tie(getBases<Base1, Base2>(c), c.a, c.b, c.s); -->

~~~~~cpp
class C : public Base1, public Base2 {
    int a, b;
    std::string s;
    friend auto hashDecompose(const C& c) {
        using namespace hashing;
        return std::tie(getBase<Base1>(c), getBase<Base2>(c), c.a, c.b, c.s);
    }
}
~~~~~

`addToHash()`
-------------

If your type needs more complicated logic, for example if you want to hash some fields conditionally, you can specify exactly what bytes should be passed to the hashing algorithm by defining a friend function with this signature:

~~~~~cpp
friend constexpr void addToHash(hashing_algorithm auto& h, const T& t) noexcept { /*...*/ }
~~~~~

This function besides your type also takes a reference to a hashing algorithm. As you can see, this parameter is constrained by a concept which you can get by including `<hashing/hash_algorithm_utils.hpp>`.
Though it is not strictly necessary it can help to detect bugs early and gives somewhat better error messages.

Inside you have to feed the hashing algorithm with the objects or bytes that you want it to hash. You can simply call recursively `addToHash()`. You can also use the variadic version

~~~~~cpp
class C: public Base1 {
    int a, b;
    std::string s;
    friend constexpr void addToHash(hashing_algorithm auto& h, const T& t) noexcept {
        // we can hash the subobject just like in hashDecompose()
        addToHash(h, getBase<Base1>(t), t.a, t.b, t.s);
        // or add some conditionally
        if (t.b > 0) {
            addToHash(h, t.s);
        }
        // or add other data
        addToHash(h, addSomePadding(), (t.a + t.b), s, size(), orAddSomeSalt());
    }
}
~~~~~

Adding type to `addToHash()` template fallback
----------------------------------------------

Sometimes you may want to hash some object which definition you have no access to. For example some types from the sandard library. In such cases you can add a 'specialization' to the `addToHash()` function template in `addToHash.hpp`.

The actual specializations or partial specializations are a bit of a mess to keep track on and maintain (the ), so instead we are using a single template with `if constexpr` conditions. If the type you want to hash is not covered already by this template, you can add a new condition there. All you have to do is to choose an appropriate place and specific enough condition so that it won't interfere with other types.


Hashing objects
===============

There main utility for hashing objects is the `Hash` class template. 


Adding new hashing algorithms
=============================

abc
