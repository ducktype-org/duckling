# Hash Module

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

This is the most common use case and also the simplest one.

The way you should think about it, is that you will, based on the state of the object to hash,
define a sequence of bytes that will be hashed with some independently chosen hashing algorithm,
and the hash obtained will be the actual hash of the object.

This means that it is possible for hashes of different types that are represented by the same sequence of bytes
to be the same (e.g. `pair<int, int>{1, 2}` and `struct{x=1, y=2}` might end up hashing to the same value).
One must be careful when using hashing for different types.

There are four ways to enable a hashing support for a class (if possible, the first two should be preferred):

`HASHING_CAN_HASH_BY_REPRESENTATION`
------------------------------------

@TODO


`hashDecompose()`
-----------------

The quickest way is to define a friend function in your class with this signature:

~~~~~cpp
friend constexpr auto hashDecompose(const T& t) noexcept { /*...*/ }
~~~~~

Inside the function all you have to do is to tell which bases/fields are part of the `observable state` of your class.
For example, `string` type should pass its `data` and `size` but not `capacity` as it is not visible in the comparisons.

To list all subfields simply return `std::tie` of all of them in order in which you would like them to be added to hash.
To pass base sub-objects you can use `getBase<Base>(t)` (defined in `<hashing/hash_algorithm_utils.hpp>`)
which casts `t` to the `Base` class and additionally checks if what you are casting to is actually a base class.

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

If your type needs more complicated logic, for example if you want to hash some fields conditionally,
you can specify exactly what bytes should be passed to the hashing algorithm by defining a friend function with this signature:

~~~~~cpp
friend constexpr void addToHash(hash_algorithm auto& h, const T& t) noexcept { /*...*/ }
~~~~~

This function, besides your type, also takes a reference to a hashing algorithm.
As you can see, this parameter is constrained by a concept which you can get by including `<hashing/hash_algorithm_utils.hpp>`.
Though it is not strictly necessary it can help to detect bugs early and gives somewhat better error messages.

Inside you have to feed the hashing algorithm with the objects or bytes that you want it to hash.
You can simply call recursively `addToHash()`. You can also use the variadic version:

~~~~~cpp
class C: public Base1, public Base2 {
    int a, b;
    std::string s;
    friend constexpr void addToHash(hash_algorithm auto& h, const T& t) noexcept {
        
        // we can hash the sub-object just like in hashDecompose()
        addToHash(h, getBase<Base1>(t), t.a, t.b, t.s, getBase<Base2>(t));

        // or add some conditionally
        if (t.b > 0) {
            addToHash(h, t.s);
        }
        
        // or add other data
        addToHash(h, addSomePadding(), (t.a + t.b), s, t.size(), orAddSomeSalt());
    }
}
~~~~~

Adding type to `addToHash()` template fallback
----------------------------------------------

Sometimes, you may want to hash an object whose definition you don’t have access to,
such as certain types from the standard library.
In such cases you can add a 'specialization' to the `addToHash()` function template in [add_to_hash.hpp](src/hashing/add_to_hash.hpp).

The actual specializations or partial specializations are [a bit of a mess](https://eel.is/c++draft/temp.expl.spec#8.sentence-2)
to keep track of and maintain - if there are many partial/full specializations interactions between them
and their placement (assuming we would like to place them in different files) would have to be considered which is often bug-prone.

Instead we are using a single template (in [add_to_hash.hpp](src/hashing/add_to_hash.hpp)) with `if constexpr` conditions,
which are much better structured, as the conditions are clearly visible and naturally create a 'control-flow' of logic.

If the type you want to hash is not covered already by this template,
you can add a new condition there. All you have to do is to choose an appropriate place
and specific enough condition so that it won't interfere with other types.


Obtaining hashes
================

The module provides two ways of hashing objects: by using either `Hash` or `StatefulHash` template classes.

`Hash`
------

This callable class wraps a hashing algorithm, providing a simple interface for hashing objects of any type with it. It also appends the corresponding type hash code after the whole object which allows to distinguish hashes of objects with the same binary representations but of different types. 

Note that it is not necessary (and so it is not done) to add hash codes after every subobject, as changing type of any of them will change the top-level type which will change the hash of the whole object.

The simplest way to use it is without specifying any template parameters.
With it's defaults it can be used as a drop-in replacement for `std::hash`:

* when hashing objects

    ~~~~~cpp
    // we can get a hash of any object by calling a temporary:
    constexpr auto h1 = Hash{}(42);
    // or by creating a hash object and calling it multiple times:
    Hash hash;
    constexpr auto h2 = hash(42.f);
    constexpr auto h3 = hash(42.0);
    ~~~~~

* or as a replacement of template parameter in unordered containers

    ~~~~~cpp
    std::unordered_map<int, int, Hash<>> m;
    m[42] = 7;
    ~~~~~

But it can also be customized.
There is one template parameters that can be specified: `HashAlgorithm`.
By specifying it we can choose the underlying algorithm that converts bytes to the hash value.

Module also provides a `DebugHash`, which instead of converting bytes to a hash value, returns a string with the bytes in hexadecimal representation and hashed objects separated with colors (red - first byte of an object).


<html>
<body>
<!--StartFragment--><html><body><!--StartFragment--><pre><div style='color: #808080; background-color: #ffffff00; font-family: Consolas, 'Courier New', monospace, monospace; font-size: 14px;'><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>7B </span><span>00 00 00 C8 01 00 00 </span></div></div></pre><!--EndFragment--></body></html><!--EndFragment-->
</body>
</html>

Using different hashing algorithms:

~~~~~cpp
auto _ x = Hash<SHA256>{}(42);
std::cout << Hash<DebugHash>{}(42) << '\n';
// prints:
~~~~~

<html>
<body>
<!--StartFragment--><html><body><!--StartFragment--><pre><div style='color: #808080; background-color: #ffffff00; font-family: Consolas, 'Courier New', monospace, monospace; font-size: 14px;'><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>2A </span><span>00 00 00                                                                                                                                                                                                           </span></</div></div></pre><!--EndFragment--></body></html><!--EndFragment-->
</body>
</html>

`StatefulHash`
--------------

This class is very similar to `Hash` but instead of hashing only a single object and immediately returning the value it keeps the state of the hashing algorithm. We can add more objects to the hash using call operator and at the end cast it to the hash value or use a `finalize()` member function. This allows for hashing multiple objects at once.

Template parameters work the same way as in `Hash`.

Example usage:
~~~~~cpp
// with one object this class works the same as `Hash`:
Hash h;
StatefulHash sh;
bool b1 = h(42) == sh(42); // true
// but now, after hashing the int, the state of sh has changed:
bool b2 = h(42) == sh(42); // false

// passing multiple objects is supported:
constexpr auto hash = StatefulHash{}(42, 3.14, "hello").finalize();
~~~~~

Similarly to `Hash` we can peek at the hashed bytes using `DebugHash`:

~~~~~cpp
std::cout << hashing::StatefulHash<hashing::DebugHash>{}(
		42,
        3.14,
        "hello",
        std::pair<std::string, char>{"abc", 'x'}
    ).finalize() << '\n';
~~~~~


`justHash()`
------------

For convenience there is also a `justHash()` function which is a shorthand for creating a temporary `Hash` or `StatefulHash` object and calling it with the given arguments.

~~~~~cpp
constexpr auto h1 = justHash(42);
constexpr auto h2 = justHash(42, 3.14, "hello");
~~~~~



Adding new hashing algorithm
============================

Adapting algorithm to the module
--------------------------------

If we want to add a new hashing algorithm we have to create a class that will split the hashing logic into three parts: setup, hashing and finalization.

To help organize it a bit there is a `hash_algorithm` concept which checks some of those properties. To satisfy it our algorithm has to be an object, have a `result_type` member type which will be returned after calling the member function `finalize()`. It also has to have a call operator can take a `std::span<const std::byte>`.

As stated before algorithm has to be organized into three stages:

1) setting up the initial state - this should happen in the constructor
2) hashing bytes - should be done in the call operator. After receiving the bytes to hash, the algorithm should update its state.
3) finalizing the hash - this should be done in the `finalize()` function returning a `result_type`; algorithm should convert it's internal state to the hash value and return it without changing it's state in the process

After the setup it should be possible to call stages 2 and 3 multiple times in any order.
