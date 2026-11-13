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

`addToHash()` as a member
-------------------------

The same hook can be a member instead of a friend:

~~~~~cpp
void addToHash(hashing::hash_algorithm auto& h) const { /*...*/ }
~~~~~

Prefer this form. A friend is found only by argument-dependent lookup, which a **qualified**
call - `hashing::addToHash(h, x)`, and most call sites in the compiler spell it that way - does
not perform; the hook is then silently skipped and the type takes an automatic path instead. A
member is found by member lookup, so it is honoured however the call is written.

`base::StrID` is the reason this branch exists: it exposes `begin`/`end`/`data`/`size`, so the
automatic range path used to claim it and its own hook never ran.

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

The hash of an object is the hash of a byte stream, so the stream has to be
**self-delimiting**: no two distinct values may flatten to the same bytes. Two rules keep it
that way.

**A range carries its element count**, as a `u64` in front of the elements. Without it two
variable-length fields hashed one after another are indistinguishable from the same bytes split
differently, and `("ab", "c")` collides with `("a", "bc")`. `u64` rather than `std::size_t` so
the hash does not depend on the platform's pointer width.

**A composite carries its member count**, as a single byte in front. `hashDecompose` and the
tuple-like path both add it. Without it `{int, int, float}` and `{float, std::string}` both
flatten to twelve zero bytes when default constructed. One byte, not eight - the count is a
compile-time constant and tiny, while a small object is only a handful of bytes of payload.

Note the asymmetry: SHA-256 covers the total stream length, so a **single** unprefixed
variable-length field is pinned by it - a fixed-size field after a string is safe. It is **two or
more** that need the prefixes. A presence flag does not help, because the flag is itself a byte a
string can imitate.


What still gives the same hash
------------------------------

The stream records shapes, not types. These still agree, by design:

* **Same-width integers of different signedness.** `u32(1)` and `i32(1)` are the same four bytes.
* **A hand-written `addToHash` that flattens differently.** The member count is added by
  `hashDecompose` and by the tuple-like path, not by a hook you wrote yourself - so a hook
  hashing two `u32`s produces the same stream as one hashing a `u64`.
* **Two composites with the same arity and the same field widths.**

`{u32, u32}` and `{u64}` reached through `hashDecompose` now differ, because the arity byte says
2 against 1 - but that is a side effect, not type awareness. Do not rely on it.

**Why this is not a problem in practice.** A query's cache key has exactly one type, and that
type is fixed for the whole compilation. The hash only ever has to separate *values of one type*
from each other, never a value of one key type from a value of another - they never share a hash
space. Within one type the encoding is unambiguous, which is what the two rules above buy.

It stops being safe the moment one hash space holds keys of more than one type. If you ever key a
single map by hashes coming from different sources, put a discriminator in the stream yourself.

### Variants

A variant hooked the way `KeyOf_MangledSymbol` does it - hash `index()`, then the active
alternative - is safe **within one variant type**: the index is a fixed-width field at a fixed
offset, so two different alternatives can never be confused, whatever their widths.

Across two *different* variant types it is the general case above. Traced by hand, with the index
as an 8-byte `usize`:

~~~~~
index=0, u64(1)              [16 B] 00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
index=1, u32(1)              [12 B] 01 00 00 00 00 00 00 00  01 00 00 00      -> differ (length)

index=0, u64(0x1_00000000)   [16 B] 00 00 00 00 00 00 00 00  00 00 00 00 01 00 00 00
index=0, u32(0) + u32(1)     [16 B] 00 00 00 00 00 00 00 00  00 00 00 00 01 00 00 00  -> SAME
~~~~~

The index never creates a hazard of its own; the collision is entirely in the payloads, and it is
the same `{u32, u32}` against `{u64}` as above.

**Nesting does not change this.** A variant holding a variant is still safe within one type - the
outer index sits at a fixed offset 0, so two different outer alternatives can never be confused
no matter how the widths stack up. Brute-forced over every combination of
`Var<Var<u32, u64>, u64>`: eleven values, zero collisions.

~~~~~
Outer[u64=1]         01 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
Outer[Inner[u32=1]]  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  01 00 00 00
Outer[Inner[u64=1]]  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
~~~~~

The one shape to watch is an **alternative that hashes no bytes at all**. Then the stream is
nothing but indices, and an index can occupy the byte positions where another type keeps data:

~~~~~
Var<u64>            holding u64(3)        00 00 00 00 00 00 00 00  03 00 00 00 00 00 00 00
Var<Var<E,E,E,E>>   inner index 3         00 00 00 00 00 00 00 00  03 00 00 00 00 00 00 00
~~~~~

Same bytes - but again two *different* types, so a query cache does not care. If you ever need
one hash space to hold both, give the empty alternative a byte of its own.


Known gaps
----------

* A **friend** `addToHash` is skipped by a qualified `hashing::addToHash(h, x)` call, because a
  qualified id performs no argument-dependent lookup. Use the member form.
* `long double` is 10 bytes of value in 16 on x87, and the floating point branch runs before the
  unique-representation check - so the same value can hash two ways depending on what was in the
  padding. Nothing in the compiler hashes one today.
* `STRONG_TYPEDEF_INT` and `STRONG_TYPEDEF_ID` are **not hashable**: the macros make classes, so
  `std::is_integral_v` is false and the type is refused outright. `u8`, `i8`, `Bytes`, `Bits` and
  every strong id are affected. `MAKE_FLAG_TYPE` opts in and works.
* `HASHING_CAN_HASH_BY_REPRESENTATION` cannot make an unsafe type byte-hashable - it is ANDed
  with `std::has_unique_object_representations_v` - but it can let through a type the trait calls
  safe while `operator==` disagrees: a `char buf[16]` with garbage past the terminator, a nested
  pointer (only a *top-level* pointer is refused), a cached or memoised field.


Obtaining hashes
================

The module provides two ways of hashing objects: by using either `Hash` or `StatefulHash` template classes.

`Hash`
------

This callable class wraps a hashing algorithm, providing a simple interface for hashing objects of any type with it.

It does **not** tag the hash with the type. Two different types whose observable state flattens to the
same bytes get the same hash - see [What the byte stream looks like](#what-the-byte-stream-looks-like)
for what that does and does not cover.

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
