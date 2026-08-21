#pragma once

#include <ser/access.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>

#include <ser/detail/describe.hpp>
#include <ser/detail/dispatch.hpp>
#include <ser/detail/meta.hpp>

#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>
#include <utility>

// ── the preprocessor plumbing ─────────────────────────────────────────────────
#define SER_DETAIL_CAT_(a, b) a##b
#define SER_DETAIL_CAT(a, b)  SER_DETAIL_CAT_(a, b)

#include <ser/detail/for_each.inc>          // SER_DETAIL_NARG, FOR_EACH, FOR_EACH_STMT

#define SER_DETAIL_STRINGIZE_(x) #x
#define SER_DETAIL_STRINGIZE(x)  SER_DETAIL_STRINGIZE_(x)
#define SER_DETAIL_QUALIFY(x)    self.x

namespace ser::detail {

    // ── what the SER_MAKE_FROM checks are made of ─────────────────────────────
    // Every one of them exists because getting it wrong is silent. A permuted argument
    // list of the same types compiles and produces a stream that reads back as garbage;
    // a missing field compiles and produces a stream one field short.

    // The argument count has to match the field count - when the field count is knowable
    // at all. For a type nobody can enumerate this is vacuously true, and then
    // SER_TEST_ROUNDTRIP is the only thing left that can catch a mistake.
    template <class T, ::std::size_t N>
    consteval bool make_from_arity_ok() {
        // A base class makes the two numbers mean different things: the declared count
        // is about this class's own members - the only ones a structured binding could
        // ever see - while the macro lists every field that goes on the wire, inherited
        // ones included. Comparing them would refuse code that works. Same reason the
        // declared-count check in describe_bind.hpp skips a type with a base.
        if constexpr (has_base_v<T>)              return true;
        else if constexpr (can_enumerate_members_v<T>) return N == member_count_v<T>;
        else                                     return true;
    }

    // Can the constructor be called with these fields, the way the macro will call it?
    //
    // std::is_constructible_v is the obvious tool and answers a different question twice
    // over. It asks about PARENTHESES, and the macro expands to BRACES: a field wider
    // than the parameter it feeds - std::size_t into an int - is an implicit conversion
    // to the one and a narrowing error to the other, so the check passes and the braces
    // below fail with the compiler's own message instead of ours. And it asks with an
    // XVALUE, while every argument the macro passes is a PRVALUE straight out of
    // dispatch_make: a field whose type cannot be moved is constructible in the code and
    // reported not constructible by the trait, which is the wrong answer about exactly
    // the types SER_MAKE_FROM exists for.
    //
    // So the question is asked in the shape it will be answered in. as_prvalue<F>() is a
    // call expression, so it IS a prvalue - unlike declval, which is an xvalue - and the
    // braces are the macro's own.
    template <class F> F as_prvalue();                      // NOT defined, on purpose

    template <class T, class... Fs>
    concept braced_from_fields = requires { T{ as_prvalue<Fs>()... }; };

    // A constructor taking std::initializer_list swallows braces: Type{a, b} would build
    // a two-element container instead of calling the two-argument constructor. Such a
    // type needs SER_MAKE_FROM_PAREN, and this is what tells it apart.
    //
    // Two routes, because neither is enough on its own.
    //
    // The direct question - is T constructible from initializer_list<E> - cannot be asked
    // without naming E, and C++ has no way to quantify over types. The only conventional
    // source for E is T::value_type, which every standard container declares. That is a
    // convention, not a language rule, and a hand-written type with an initializer_list
    // constructor and no such typedef is invisible to it.
    //
    // So the second route asks the question that actually matters: does a BRACED list of
    // ANY length construct this type? A normal type stops at its field count; one whose
    // braces are a list runs past the ladder's limit, and the probe reports it. That
    // needs no convention and no element type.
    //
    // Braced, and that word is load-bearing. A variadic constructor template also takes
    // any number of arguments, but parentheses reach it identically - so it is not this
    // trap, and telling its author to use SER_MAKE_FROM_PAREN would change nothing. What
    // keeps the two apart for free is that a braced-init-list is a non-deduced context:
    // Args... cannot be deduced from {p}, so the braced probe stops at 0 for a variadic
    // constructor and runs to the limit for a list.
    template <class T>
    consteval bool has_initializer_list_ctor() {
        if constexpr (requires { typename T::value_type; })
            if (::std::is_constructible_v<T, ::std::initializer_list<typename T::value_type>>)
                return true;
        return arity_saturated_v<T>;
    }

    template <class T>
    inline constexpr bool has_initializer_list_ctor_v = has_initializer_list_ctor<T>();

} // namespace ser::detail

// -- the two field shapes a hook has to refuse for itself ----------------------
// The walk asks detail::check_walkable_fields, but a type with a hook never reaches the
// walk - the hook IS the format. A hand-written ser_visit can therefore serialize a
// reference or a bit-field with nothing to stop it, and there is no way to catch that:
// by the time `ar(x, y)` runs the arguments are expressions, and an expression carries
// neither property.
//
// A MACRO can, because it still has the field NAMES. decltype of a member access is the
// DECLARED type, so a reference is visible; a bit-field is visible by asking whether a
// non-const lvalue reference binds to it. Both go through declval<Self&>() rather than
// `self`, because the write side takes self by const reference and a const reference
// binds to a bit-field perfectly well.
//
// So the macros check and a hand-written hook does not, which is the honest asymmetry:
// naming a field is what makes the question askable at all.
#define SER_DETAIL_FIELD_REF_OK(x)                                                         \
    static_assert(!::std::is_reference_v<                                              \
                      decltype(::std::declval<ser_detail_field_self_t&>().x)>,         \
        "ser: cannot serialize a REFERENCE field. Reading has to overwrite the object " \
        "and a reference can never be rebound. Hold the value itself, or leave this "   \
        "field out of the list.");

#define SER_DETAIL_FIELD_BITS_OK(x)                                                     \
    static_assert(requires { ::ser::access::bind_probe(                                \
                      ::std::declval<ser_detail_field_self_t&>().x); },                \
        "ser: cannot serialize a BIT-FIELD through ser_visit. ser_visit reads by writing " \
        "through a reference to each field, and nothing binds a reference to a bit-field. " \
        "The automatic member walk handles a bit-field by BUILDING the object instead, so " \
        "there are three ways out: drop SER_DESCRIBE and let the walk do it, use "      \
        "SER_DESCRIBE_MAKE(Type, a, b) whose read side builds, or leave this field out of " \
        "the list. The wire widens a bit-field to its declared type either way.");

// The two halves together, for a hook that reads by writing through a reference.
#define SER_DETAIL_FIELD_OK(x)                                                          \
    SER_DETAIL_FIELD_REF_OK(x)                                                          \
    SER_DETAIL_FIELD_BITS_OK(x)


// The write side of SER_DESCRIBE_MAKE needs only the reference half: its read side is
// ser_make, which BUILDS, and braces initialize a bit-field by value - so a bit-field is
// perfectly serializable through that pair. ser_visit is the one that cannot, because it
// reads by writing through a reference and no reference binds to a bit-field.
#define SER_DETAIL_FIELD_CHECKS_BUILT(...)                                              \
    using ser_detail_field_self_t = ::std::remove_cvref_t<decltype(self)>;              \
    SER_DETAIL_FOR_EACH_STMT(SER_DETAIL_FIELD_REF_OK, __VA_ARGS__)

#define SER_DETAIL_FIELD_CHECKS(...)                                                   \
    using ser_detail_field_self_t = ::std::remove_cvref_t<decltype(self)>;             \
    SER_DETAIL_FOR_EACH_STMT(SER_DETAIL_FIELD_OK, __VA_ARGS__)

// ── SER_DESCRIBE ──────────────────────────────────────────────────────────────
// Lists the fields that go on the wire, in order, and skips the rest. It expands to an
// in-class ser_visit, so the ladder finds it at level 2 and nothing else has to know
// about it - and to the field names, which is what lets SER_TEST_ROUNDTRIP say which
// field came back wrong instead of just "not equal".
//
// Needs at least one field; a type with nothing to serialize needs no description.
// The half that only DESCRIBES: the count, the names, and the fields as a tuple. It
// says nothing about the format, so it can sit under either of the two hook forms
// below - the symmetric ser_visit, or ser_write together with ser_make.
#define SER_DETAIL_DESCRIBE_FIELDS(...)                                                \
    static constexpr ::std::size_t ser_field_count = SER_DETAIL_NARG(__VA_ARGS__);     \
    static constexpr const char* ser_field_names[SER_DETAIL_NARG(__VA_ARGS__)] = {     \
        SER_DETAIL_FOR_EACH(SER_DETAIL_STRINGIZE, __VA_ARGS__)                         \
    };                                                                                 \
    static constexpr auto ser_described(auto& self) {                                  \
        return ::std::tie(SER_DETAIL_FOR_EACH(SER_DETAIL_QUALIFY, __VA_ARGS__));       \
    }

#define SER_DESCRIBE(...)                                                              \
    SER_DETAIL_DESCRIBE_FIELDS(__VA_ARGS__)                                            \
    static constexpr ::ser::errc ser_visit(auto& ar, auto& self) {                     \
        SER_DETAIL_FIELD_CHECKS(__VA_ARGS__)                                           \
        return ar(SER_DETAIL_FOR_EACH(SER_DETAIL_QUALIFY, __VA_ARGS__));               \
    }

// ── SER_MAKE_FROM ─────────────────────────────────────────────────────────────
// The hook for a type that cannot be filled in after the fact: const fields, no default
// constructor, no move constructor. Names the constructor arguments in order.
//
//     class Point {
//         const int x, y;
//     public:
//         Point(int a, int b) : x(a), y(b) {}
//         SER_MAKE_FROM(Point, x, y)
//         static ser::errc ser_write(ser::writer auto& ar, const Point& p) { ... }
//     };
//
// No local variables anywhere in the expansion: each argument is a prvalue that
// initializes its parameter directly, so nothing is ever moved.
#define SER_DETAIL_MAKE_TYPE(x) ::std::remove_cv_t<decltype(ser_detail_self_t::x)>
#define SER_DETAIL_MAKE_ARG(x)  ::ser::read_field<SER_DETAIL_MAKE_TYPE(x)>(ar)

#define SER_DETAIL_MAKE_FROM_CHECKS(Self, ...)                                         \
    using ser_detail_self_t = Self;                                                    \
    static_assert(::ser::detail::make_from_arity_ok<Self, SER_DETAIL_NARG(__VA_ARGS__)>(), \
        "ser: SER_MAKE_FROM lists a different number of fields than this type has. "    \
        "List every field that goes on the wire, in declaration order.");               \
    static_assert(!::ser::detail::has_initializer_list_ctor_v<Self>,                    \
        "ser: this type accepts a BRACED list of any length - a std::initializer_list "  \
        "constructor - so the braces SER_MAKE_FROM expands to would be swallowed as a "  \
        "list instead of calling the constructor with the fields as arguments. Use "     \
        "SER_MAKE_FROM_PAREN(Type, a, b) - it uses parentheses.");                       \
    static_assert(::ser::detail::braced_from_fields<Self,                              \
                      SER_DETAIL_FOR_EACH(SER_DETAIL_MAKE_TYPE, __VA_ARGS__)>,          \
        "ser: this type cannot be built from its fields, in braces, in the order "      \
        "given. Check the order against the constructor - two fields of different "     \
        "types swapped is exactly what this catches. If the order is right, the other " \
        "cause is a narrowing conversion, which braces refuse and parentheses allow: "  \
        "SER_MAKE_FROM_PAREN(Type, a, b) accepts it, or widen the parameter.");

#define SER_MAKE_FROM(Self, ...)                                                       \
    static Self ser_make(::ser::reader auto& ar) {                                      \
        SER_DETAIL_MAKE_FROM_CHECKS(Self, __VA_ARGS__)                                  \
        return Self{ SER_DETAIL_FOR_EACH(SER_DETAIL_MAKE_ARG, __VA_ARGS__) };            \
    }

// Same, without the field list: the fields come from the structured-bindings walk, so
// the type has to be enumerable. It cannot check the order of the constructor arguments
// against anything, which is why SER_TEST_ROUNDTRIP is a CONDITION of using it and not
// a suggestion.
#define SER_MAKE_FROM_MEMBERS(Self)                                                    \
    static Self ser_make(::ser::reader auto& ar) {                                      \
        static_assert(::ser::can_enumerate_members_v<Self>,                             \
            "ser: SER_MAKE_FROM_MEMBERS needs to know this type's fields. Declare "     \
            "using ser_members = ser::members<N>; or list them: SER_MAKE_FROM(Type, a, b).");  \
        static_assert(!::ser::detail::has_initializer_list_ctor_v<Self>,                \
            "ser: this type accepts a BRACED list of any length - a std::initializer_list " \
            "constructor - and SER_MAKE_FROM_MEMBERS expands to braces exactly as "     \
            "SER_MAKE_FROM does, so the fields would be swallowed as a list instead of " \
            "reaching the constructor. Use SER_MAKE_FROM_PAREN(Type, a, b) - it uses "  \
            "parentheses. SER_MAKE_FROM_MEMBERS_PAREN(Type) is the same thing without a "     \
            "field list, which this type does not need.");                            \
        return ::ser::detail::make_by_fields<Self>(ar);                                  \
    }

// Fields from the walk, constructor through parentheses. The two reasons to reach for
// the other variants at once: nothing to list, and braces that would be swallowed.
#define SER_MAKE_FROM_MEMBERS_PAREN(Self)                                              \
    static Self ser_make(::ser::reader auto& ar) {                                      \
        static_assert(::ser::can_enumerate_members_v<Self>,                             \
            "ser: SER_MAKE_FROM_MEMBERS_PAREN needs to know this type's fields. Declare " \
            "using ser_members = ser::members<N>; or list them: SER_MAKE_FROM_PAREN(Type, a, b).");  \
        return ::ser::detail::make_paren_by_fields<Self>(ar);                            \
    }

// ── SER_DESCRIBE_MAKE ─────────────────────────────────────────────────────────
// The third of the three hook forms, which was the only one without a macro.
//
//   SER_DESCRIBE      - ser_visit:            one function, both directions, and the
//                       object has to exist before it can be filled in.
//   SER_DESCRIBE_MAKE - ser_write + ser_make: same field list, both directions, and the
//                       object is BUILT on the way in - const fields, no default
//                       constructor, no move constructor.
//
// One line, and it needs no ser_members: the field list is the type's own answer to
// "which fields, in what order", so nothing has to be counted. That is also why
// SER_TEST_ROUNDTRIP can name the field that came back wrong here without any further
// declaration.
//
//     class Reading {
//         const std::uint32_t sensor;
//         float value;
//     public:
//         Reading(std::uint32_t s, float v) : sensor(s), value(v) {}
//         SER_FRIEND
//     private:
//         SER_DESCRIBE_MAKE(Reading, sensor, value)
//     };
//
// Not to be combined with SER_DESCRIBE on one type: both define ser_field_names and
// ser_described, so the compiler reports a redefinition. That is the right answer for
// the wrong-looking reason, and the two are alternatives anyway - a type whose ser_visit
// can fill it in does not need a ser_make, and one that needs a ser_make cannot be
// filled in. The library refuses the pair on its own merits as well: ser_visit together
// with ser_write is a format that disagrees with itself, and check_hooks says so.
#define SER_DESCRIBE_MAKE(Self, ...)                                                   \
    SER_DETAIL_DESCRIBE_FIELDS(__VA_ARGS__)                                            \
    static constexpr ::ser::errc ser_write(::ser::writer auto& ar, const Self& self) {            \
        SER_DETAIL_FIELD_CHECKS_BUILT(__VA_ARGS__)                                           \
        return ar(SER_DETAIL_FOR_EACH(SER_DETAIL_QUALIFY, __VA_ARGS__));               \
    }                                                                                  \
    SER_MAKE_FROM(Self, __VA_ARGS__)

// The same for a constructor that would swallow the braces - see SER_MAKE_FROM_PAREN.
// The write side is unaffected by that; only the read side changes.
#define SER_DESCRIBE_MAKE_PAREN(Self, ...)                                             \
    SER_DETAIL_DESCRIBE_FIELDS(__VA_ARGS__)                                            \
    static constexpr ::ser::errc ser_write(::ser::writer auto& ar, const Self& self) {            \
        SER_DETAIL_FIELD_CHECKS_BUILT(__VA_ARGS__)                                           \
        return ar(SER_DETAIL_FOR_EACH(SER_DETAIL_QUALIFY, __VA_ARGS__));               \
    }                                                                                  \
    SER_MAKE_FROM_PAREN(Self, __VA_ARGS__)

// Parentheses instead of braces, for a constructor that would otherwise be shadowed by
// std::initializer_list. Parentheses do not order their arguments, so the fields are
// read into locals first - declarations, which ARE ordered - and moved in. That move is
// the price, and it is why this is not the default.
#define SER_DETAIL_MAKE_LOCAL(x)                                                       \
    auto ser_local_##x = ::ser::read_field<SER_DETAIL_MAKE_TYPE(x)>(ar);
#define SER_DETAIL_MOVE_LOCAL(x) ::std::move(ser_local_##x)

#define SER_MAKE_FROM_PAREN(Self, ...)                                                 \
    static Self ser_make(::ser::reader auto& ar) {                                      \
        using ser_detail_self_t = Self;                                                 \
        static_assert(::ser::detail::make_from_arity_ok<Self, SER_DETAIL_NARG(__VA_ARGS__)>(), \
            "ser: SER_MAKE_FROM_PAREN lists a different number of fields than this "    \
            "type has. List every field that goes on the wire, in declaration order."); \
        SER_DETAIL_FOR_EACH_STMT(SER_DETAIL_MAKE_LOCAL, __VA_ARGS__)                    \
        return Self( SER_DETAIL_FOR_EACH(SER_DETAIL_MOVE_LOCAL, __VA_ARGS__) );          \
    }
