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

There are four ways to enable a hashing support for a class (if possible, the first three should be preferred):

`HASHING_CAN_HASH_BY_REPRESENTATION`
------------------------------------

If your type has a unique representation in memory, you can simply add a static member to it:
~~~~~cpp
struct S {
    u64 x;
    static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};
};
~~~~~

With this member, the module will know that it can treat your type as a sequence of bytes and hash it directly.
Note that this might now always be the correct way to hash your type, even if it has a unique representation.


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


Hashing pointers - `HashByAddress`
==============================

Raw pointers are **not** hashable on their own: hashing an address is almost never what one wants,
since addresses are not stable between runs and say nothing about the pointee
(e.g. a `const char*` would hash to something completely unrelated to the string it points to).
Trying to hash one is a compile error telling you to pick an intent.

If you do mean the address - i.e. the *identity* of the pointee - wrap it in the
`HashByAddress` proxy from [by_address.hpp](src/hashing/by_address.hpp):

~~~~~cpp
justHash(ptr);                 // ill-formed
justHash(*ptr);                // hashes the pointee
justHash(HashByAddress{ ptr });    // hashes the address
~~~~~

The same applies to pointers nested inside other objects: a `hashDecompose()` that ties a raw
pointer member, or a contiguous range of pointers, is rejected as well. Watch out for implicit
array-to-pointer decay - `std::tuple{ 42, "hello" }` deduces a `const char*` member,
so use `std::string_view{ "hello" }` (or `std::string`) when you want the characters hashed.


What the byte stream looks like
==============================

The hash of an object is the hash of a byte stream, and that stream has to be
**self-delimiting**: no two distinct values may flatten to the same bytes.

There is exactly **one entry point** through which payload bytes reach an algorithm -
`hashing::internal::addBytes()` - and it always writes the **length first**, then the bytes.
Every automatic path and every hand-written hook funnels through it, so the guarantee does not
depend on anybody remembering it. That is the whole design: not two helpers where you have to
know which one frames and which one does not.

~~~~~
<length> <bytes>          every field, without exception
~~~~~

Because the length is in front of *every* field and not only the variable-length ones, all of
these are separated:

* two adjacent strings - `("ab", "c")` no longer collides with `("a", "bc")`
* a `{u32, u32}` against a `{u64}` - the first says `4|.. 4|..`, the second `8|..`
* a range against the same bytes reached any other way

Two fields are **framing** rather than payload and are written by the framing layer directly: a
composite's arity and, on the element-by-element range path, the element count. The grammar
says where they appear, so the reader expects them and they need no length of their own - the
same reasoning that makes a length prefix unable to carry a length prefix.

### The length encoding

One byte for a length up to `0xFE`; otherwise the escape byte `0xFF` followed by a
little-endian `u64`. Canonical because the rule leaves no choice - below `0xFF` the short form
is required and the escape is forbidden, so every length has exactly one spelling. A scheme
allowing two spellings of the same length would put the ambiguity back into the very field
meant to remove it.

This is not only cheaper than a fixed eight-byte length, it is cheaper than **no** universal
prefix at all. Measured with `perf` on a 63k line package, `-O3`:

~~~~~
                                  SHA256::transform    all of hashing::
prefix on ranges only (before)          8.80%              11.28%
universal prefix, fixed u64             9.43%              12.10%      +0.82 pp
universal prefix, 1 byte + escape       7.28%               9.75%      -1.53 pp
~~~~~

Adding prefixes everywhere costs, exactly as you would expect - that is the `+0.82 pp`. What
pays for it is narrowing the prefix that *already* existed on every range. The compiler hashes
mostly identifiers, five to fifteen characters, where an eight-byte length was about as large
as the data it described; and SHA-256 charges per 64-byte block, so shaving seven bytes off
several prefixes in one key drops a great many streams from two blocks to one. Net effect on a
full compile: **1.5% faster than before the change**.

For context, `hashing::SHA256::transform` is the single hottest symbol in `duckc`.

### Writing an algorithm, or a hook

An algorithm receives **bytes and nothing else**. Its byte sink is called `update()`, not
`operator()`, precisely so that it does not read like "hash this object" - a hook author who
wrote `h(something)` used to hand over unframed bytes and quietly lose the length prefix.

Inside a hook, never touch the algorithm. Call `addToHash()` on the things you want hashed and
the framing takes care of itself:

~~~~~cpp
// yes
void addToHash(hashing::hash_algorithm auto& h) const { hashing::addToHash(h, a, b); }

// no - unframed bytes, and it will not compile
void addToHash(hashing::hash_algorithm auto& h) const { h(asBytes(a)); }
~~~~~


What still gives the same hash
------------------------------

The stream records shapes, not types. These still agree, by design:

* **Same-width integers of different signedness.** `u32(1)` and `i32(1)` are the same four bytes.
* **Two composites with the same arity and the same field widths.**
* **A hand-written hook that flattens differently** from another type's hook.

**Why this is not a problem in practice.** A query's cache key has exactly one type, fixed for
the whole compilation. The hash only ever has to separate *values of one type* from each other,
never a value of one key type from a value of another - they never share a hash space. Within
one type the encoding is unambiguous, which is what the rule above buys.

It stops being safe the moment one hash space holds keys of more than one type. If you ever key
a single map by hashes coming from different sources, put a discriminator in the stream
yourself.


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

To help organize it a bit there is a `hash_algorithm` concept which checks some of those properties. To satisfy it our algorithm has to be an object, have a `result_type` member type which will be returned after calling the member function `finalize()`. It also has to have an `update()` member function taking a `std::span<const std::byte>`.

Note that `update()` is the algorithm's *private* interface with the module: only `hashing::internal::addBytes()` is meant to call it, and it receives bytes that already carry a length prefix. An algorithm never sees objects, only bytes.

As stated before algorithm has to be organized into three stages:

1) setting up the initial state - this should happen in the constructor
2) hashing bytes - should be done in `update()`. After receiving the bytes to hash, the algorithm should update its state.
3) finalizing the hash - this should be done in the `finalize()` function returning a `result_type`; algorithm should convert it's internal state to the hash value and return it without changing it's state in the process

After the setup it should be possible to call stages 2 and 3 multiple times in any order.
