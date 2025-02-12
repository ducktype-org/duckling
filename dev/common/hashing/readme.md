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

Inside you have to feed the hashing algorithm with the objects or bytes that you want it to hash. You can simply call recursively `addToHash()`. You can also use the variadic version:

~~~~~cpp
class C: public Base1, public Base2 {
    int a, b;
    std::string s;
    friend constexpr void addToHash(hashing_algorithm auto& h, const T& t) noexcept {
        // we can hash the subobject just like in hashDecompose()
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

Sometimes, you may want to hash an object whose definition you don’t have access to, such as certain types from the standard library. In such cases you can add a 'specialization' to the `addToHash()` function template in `addToHash.hpp`.

The actual specializations or partial specializations are a bit of a mess to keep track of and maintain (there are a lot of interactions to consider), so instead we are using a single template with `if constexpr` conditions. If the type you want to hash is not covered already by this template, you can add a new condition there. All you have to do is to choose an appropriate place and specific enough condition so that it won't interfere with other types.


Obtaining hashes
================

The module provides two ways of hashing objects: by using either `Hash` or `StatefulHash` template classes.

`Hash`
------

This callable class wraps a hashing algorithm, providing a simple interface for hashing objects of any type with it. It also appends the corresponding type hash code which allows to distinguish hashes of objects with the same binary representations but of different types.

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
There are two template parameters that can be specified: `HashAlgorithm` and `TypeHC`.

* The first one chooses the underlying algorithm that converts bytes to the hash value.

    Module provides a generic, constexpr implementation of `Fnv1a` which is a fast and simple hashing algorithm with a good enough distribution for most applications like hash tables. It is available in its 32 bit version as `Fnv1a_32` and 64 bit version as `Fnv1a_64` which is also the default algorithm used by `Hash`.

    There is also a `DebugHash`, which instead of converting bytes to a hash value, returns a string with the bytes in hexadecimal representation and hashed objects separated with colors (red - first byte of an object, yellow - first byte of the hash code).
    
    <html>
    <body>
    <!--StartFragment--><html><body><!--StartFragment--><pre><div style='color: #808080; background-color: #ffffff00; font-family: Consolas, 'Courier New', monospace, monospace; font-size: 14px;'><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>7B </span><span>00 00 00 C8 01 00 00 </span><span style='color: #e5e510; font-weight: bold;'>F5 </span><span>4B 51 5C   </span></div></div></pre><!--EndFragment--></body></html><!--EndFragment-->
    </body>
    </html>

* The second one specifies what should be appended to the hashed bytes of the object. Allowed types are specializations of `TypeHashCodeBase` or the type `void`. Shorter hash codes of may be desired when hashing many small objects. If `void` is used, no bytes are appended after the object.

Using different hashing algorithms:
~~~~~cpp
// `Hash` is the same as `Hash<Fnv1a_64>`
bool b = Hash{}(42) == Hash<Fnv1a_64, void>{}(42); // true
// If many hashes are stored we can use shorter ones:
u32 hash = Hash<Fnv1a_32>{}(42);
~~~~~

Different hash type code lengths used:
~~~~~cpp
std::cout << Hash<DebugHash, void>{}(42) << '\n'
          << Hash<DebugHash>{}(42) << '\n'
          << Hash<DebugHash, TypeHashCodeBase<u64>>{}(42) << '\n';
// prints:
~~~~~
<html>
<body>
<!--StartFragment--><html><body><!--StartFragment--><pre><div style='color: #808080; background-color: #ffffff00; font-family: Consolas, 'Courier New', monospace, monospace; font-size: 14px;'><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>2A </span><span>00 00 00                                                                                                                                                                                                           </span></div><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>2A </span><span>00 00 00 </span><span style='color: #e5e510; font-weight: bold;'>19 </span><span>65 94 2A                                                                                                                                                                                               </span></div><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>2A </span><span>00 00 00 </span><span style='color: #e5e510; font-weight: bold;'>F5 </span><span>DD 91 3F 7A 2E 2B 44 </span></div></div></pre><!--EndFragment--></body></html><!--EndFragment-->
</body>
</html>


`StatefulHash`
--------------

This class is very similar to `Hash` but instead of hashing only a single object and immediately returning the value it also keeps the state of the hashing algorithm. We can add more objects to the hash using call operator and at the end cast it to the hash value. This allows for hashing multiple objects at once.

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
constexpr auto hash = StatefulHash{}(42, 3.14, "hello");
~~~~~

Similarly to `Hash` we can peek at the hashed bytes using `DebugHash`:

~~~~~cpp
std::cout << hashing::StatefulHash<hashing::DebugHash>{}(
		42,
        3.14,
        "hello",
        std::pair<std::string, char>{"abc", 'x'
    ) << '\n';
// prints:
~~~~~
<html>
<body>
<!--StartFragment--><html><body><!--StartFragment--><pre><div style='color: #808080; background-color: #ffffff00; font-family: Consolas, 'Courier New', monospace, monospace; font-size: 14px;'><div><span>line    0:    </span><span style='color: #cd3131; font-weight: bold;'>2A </span><span>00 00 00 </span><span style='color: #e5e510; font-weight: bold;'>19 </span><span>65 94 2A </span><span style='color: #cd3131; font-weight: bold;'>1F </span><span>85 EB 51 B8 1E 09 40                                                                                                                                                                       </span></div><div><span>line    1:    </span><span style='color: #e5e510; font-weight: bold;'>7B </span><span>FE 1A 9C </span><span style='color: #cd3131; font-weight: bold;'>68 </span><span>65 6C 6C 6F 00 </span><span style='color: #e5e510; font-weight: bold;'>DA </span><span>F6 8B A0 </span><span style='color: #cd3131; font-weight: bold;'>61 </span><span>62                                                                                                                                                                       </span></div><div><span>line    2:    63 </span><span style='color: #cd3131; font-weight: bold;'>78 </span><span style='color: #e5e510; font-weight: bold;'>89 </span><span>B4 D7 D7</span></div></div></pre><!--EndFragment--></body></html><!--EndFragment-->
</body>
</html>


`TYPE_HASH_CODE`
----------------

Module also provides a `TYPE_HASH_CODE` variable template which is a unique integer constant for each type. In contrast to `std::type_info::hash_code()` it can be used in a `constexpr` context and in templates. It is used by `Hash` and `StatefulHash` to append appropriate bytes to the hashed bytes.

It can be used simply by providing it the type we are interested in:
~~~~~cpp
static_assert(TYPE_HASH_CODE<int> != TYPE_HASH_CODE<float>);
~~~~~
We can also specify the size of the hash code:
~~~~~cpp
bool b1 = sizeof(TYPE_HASH_CODE<int, u32>) == 4; // true
bool b2 = sizeof(TYPE_HASH_CODE<int, u64>) == 8; // true
~~~~~

Adding new hashing algorithm
============================

Adapting algorithm to the module
--------------------------------

If we want to add a new hashing algorithm we have to create a class that will split the hashing logic into three parts: setup, hashing and finalization.

To help organize it a bit there is a `hash_algorithm` concept which checks some of those properties. To satisfy it our algorithm has to be an object, have a `result_type` to which it can be explicitly converted to and be callable with `(void*, usize)` or `(std::string_view)`.

As stated before algorithm has to be organized into three stages:

1) setting up the initial state - this should happen in the constructor
2) hashing bytes - should be done in the call operator. After receiving the bytes to hash, the algorithm should update its state.
3) finalizing the hash - this should be done in the conversion operator to the `result_type`.

After the setup it should be possible to call stages 2 and 3 multiple times in any order.

CallOverloads utility
---------------------

This is a class that, when derived from publicly, adds call operator overloads to the derived class.

To use it, our class should have a protected member function `updateHash(void*, usize)` or `updateHash(std::string_view)` which will be called by the call operator. It also shouldn't be final.
It can also have a protected member function `addHashCode(any TypeHashCode)` which will be called if matching `TypeHashCode` is passed.
When possible call operator will be constexpr.
Class also adds optimal overloads for types which values have unique representations in memory.
