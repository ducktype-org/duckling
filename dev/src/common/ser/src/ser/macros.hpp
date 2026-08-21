#pragma once

#include <base/preproc/for_each.hpp>
#include <base/preproc/stringify.hpp>

#include <ser/access.hpp>
#include <ser/concepts.hpp>
#include <ser/detail/describe.hpp>
#include <ser/detail/dispatch.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

// ── the preprocessor plumbing ─────────────────────────────────────────────────
// FOR_EACH and FOR_EACH_COMMA come from base: statements and an argument list
// respectively. The field count is an EXPRESSION rather than a token, which is all any
// caller here needs it to be.
#define SER_DETAIL_PLUS_ONE(x)      +1
#define SER_DETAIL_FIELD_COUNT(...) (::std::size_t{ 0 } FOR_EACH(SER_DETAIL_PLUS_ONE, __VA_ARGS__))

#define SER_DETAIL_QUALIFY(x) self.x

namespace ser::detail {

	// ── what the SER_MAKE_FROM checks are made of ─────────────────────────────
	// Every one of them exists because getting it wrong is silent. A permuted argument
	// list of the same types compiles and produces a stream that reads back as garbage;
	// a missing field compiles and produces a stream one field short.

	// The argument count has to match the field count - when the field count is knowable
	// at all. For a type nobody can enumerate this is vacuously true, and then
	// SER_TEST_ROUNDTRIP is the only thing left that can catch a mistake.
	template<class T, ::std::size_t N>
	consteval bool makeFromArityOk() {
		// A base class makes the two numbers mean different things: the declared count is
		// about this class's own members - the only ones a structured binding could ever
		// see - while the macro lists every field that goes on the wire, inherited ones
		// included. Comparing them would refuse code that works.
		if constexpr (HAS_BASE_V<T>)
			return true;
		else if constexpr (CAN_ENUMERATE_MEMBERS_V<T>)
			return N == MEMBER_COUNT_V<T>;
		else
			return true;
	}

	// Can the constructor be called with these fields, the way the macro will call it?
	//
	// std::is_constructible_v is the obvious tool and answers a different question twice
	// over. It asks about PARENTHESES while the macro expands to BRACES, so a narrowing
	// conversion passes the check and fails the code; and it asks with an XVALUE while
	// every argument the macro passes is a PRVALUE out of dispatchMake, so a field that
	// cannot be moved is reported unusable although it works.
	//
	// So the question is asked in the shape it will be answered in. asPrvalue<F>() is a
	// call expression, so it IS a prvalue - unlike declval, which is an xvalue - and the
	// braces are the macro's own.
	template<class F>
	F asPrvalue();  // NOT defined, on purpose

	template<class T, class... Fs>
	concept braced_from_fields = requires { T{ asPrvalue<Fs>()... }; };

	// A constructor taking std::initializer_list swallows braces: Type{a, b} would build
	// a two-element container instead of calling the two-argument constructor. Such a
	// type needs SER_MAKE_FROM_PAREN, and this is what tells it apart.
	//
	// Two routes, because neither is enough on its own. Asking directly needs the element
	// type, and the only conventional source for it is T::value_type - which a
	// hand-written type with an initializer_list constructor need not have. So the second
	// route asks whether a BRACED list of ANY length constructs the type: a normal type
	// stops at its field count, a list never stops. Braced is load-bearing - a variadic
	// constructor template also takes any number of arguments, but a braced-init-list is
	// a non-deduced context, so the probe stops at 0 for it.
	template<class T>
	consteval bool hasInitializerListCtor() {
		if constexpr (requires { typename T::value_type; })
			if (::std::is_constructible_v<T, ::std::initializer_list<typename T::value_type>>)
				return true;
		return ARITY_SATURATED_V<T>;
	}

	template<class T>
	inline constexpr bool HAS_INITIALIZER_LIST_CTOR_V = hasInitializerListCtor<T>();

}  // namespace ser::detail

// ── the two field shapes a hook has to refuse for itself ──────────────────────
// A type with a hook never reaches the automatic walk, so the walk's own field checks
// never run for it - and by the time `ar(x, y)` runs the arguments are expressions, which
// carry neither of these properties. A MACRO can still see the field NAMES, and that is
// what makes the question askable: decltype of a member access is the DECLARED type, and
// a bit-field is the one thing no non-const lvalue reference binds to.
//
// Both go through declval<Self&>() rather than `self`, because the write side takes self
// by const reference and a const reference binds to a bit-field perfectly well.
#define SER_DETAIL_FIELD_REF_OK(x)                                                      \
	static_assert(                                                                      \
		!::std::is_reference_v<decltype(::std::declval<ser_detail_field_self_t&>().x)>, \
		"ser: cannot serialize a REFERENCE field. Reading has to overwrite the object " \
		"and a reference can never be rebound. Hold the value itself, or leave this "   \
		"field out of the list."                                                        \
	);

#define SER_DETAIL_FIELD_BITS_OK(x)                                                           \
	static_assert(                                                                            \
		requires { ::ser::access::bindProbe(::std::declval<ser_detail_field_self_t&>().x); }, \
		"ser: cannot serialize a BIT-FIELD through serVisit. serVisit reads by writing "      \
		"through a reference to each field, and nothing binds a reference to a bit-field. "   \
		"The automatic member walk handles a bit-field by BUILDING the object instead, so "   \
		"there are three ways out: drop SER_DESCRIBE and let the walk do it, use "            \
		"SER_DESCRIBE_MAKE(Type, a, b) whose read side builds, or leave this field out of "   \
		"the list. The wire widens a bit-field to its declared type either way."              \
	);

// The two halves together, for a hook that reads by writing through a reference.
#define SER_DETAIL_FIELD_OK(x) \
	SER_DETAIL_FIELD_REF_OK(x) \
	SER_DETAIL_FIELD_BITS_OK(x)

// The write side of SER_DESCRIBE_MAKE needs only the reference half: its read side is
// serMake, which BUILDS, and braces initialize a bit-field by value.
#define SER_DETAIL_FIELD_CHECKS_BUILT(...)                                 \
	using ser_detail_field_self_t = ::std::remove_cvref_t<decltype(self)>; \
	FOR_EACH(SER_DETAIL_FIELD_REF_OK, __VA_ARGS__)

#define SER_DETAIL_FIELD_CHECKS(...)                                       \
	using ser_detail_field_self_t = ::std::remove_cvref_t<decltype(self)>; \
	FOR_EACH(SER_DETAIL_FIELD_OK, __VA_ARGS__)

// ── SER_DESCRIBE ──────────────────────────────────────────────────────────────
// Lists the fields that go on the wire, in order, and skips the rest. It expands to an
// in-class serVisit, so dispatch finds it at level 2 and nothing else has to know about
// it - and to the field names, which is what lets SER_TEST_ROUNDTRIP say which field came
// back wrong instead of just "not equal". Needs at least one field.

// The half that only DESCRIBES: the count, the names, and the fields as a tuple. It says
// nothing about the format, so it sits under either of the two hook forms below.
//
// std::array rather than a C array, and that is about the CALLER rather than about this
// file: clang-tidy reports cppcoreguidelines-avoid-c-arrays at the line that uses the
// macro, so a C array here would make every type that says SER_DESCRIBE carry a NOLINT.
#define SER_DETAIL_DESCRIBE_FIELDS(...)                                                             \
	static constexpr ::std::size_t ser_field_count = SER_DETAIL_FIELD_COUNT(__VA_ARGS__);           \
	static constexpr ::std::array<const char*, SER_DETAIL_FIELD_COUNT(__VA_ARGS__)> ser_field_names \
		= { FOR_EACH_COMMA(STRINGIFY_2, __VA_ARGS__) };                                             \
	static constexpr auto ser_described(auto& self) {                                               \
		return ::std::tie(FOR_EACH_COMMA(SER_DETAIL_QUALIFY, __VA_ARGS__));                         \
	}

#define SER_DESCRIBE(...)                                           \
	SER_DETAIL_DESCRIBE_FIELDS(__VA_ARGS__)                         \
	static constexpr ::ser::Errc serVisit(auto& ar, auto& self) {   \
		SER_DETAIL_FIELD_CHECKS(__VA_ARGS__)                        \
		return ar(FOR_EACH_COMMA(SER_DETAIL_QUALIFY, __VA_ARGS__)); \
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
//         static ser::Errc serWrite(ser::writer auto& ar, const Point& p) { ... }
//     };
//
// No local variables anywhere in the expansion: each argument is a prvalue that
// initializes its parameter directly, so nothing is ever moved.
#define SER_DETAIL_MAKE_TYPE(x) ::std::remove_cv_t<decltype(ser_detail_self_t::x)>
#define SER_DETAIL_MAKE_ARG(x)  ::ser::readField<SER_DETAIL_MAKE_TYPE(x)>(ar)

#define SER_DETAIL_MAKE_FROM_CHECKS(Self, ...)                                                      \
	using ser_detail_self_t = Self;                                                                 \
	static_assert(                                                                                  \
		::ser::detail::makeFromArityOk<Self, SER_DETAIL_FIELD_COUNT(__VA_ARGS__)>(),                \
		"ser: SER_MAKE_FROM lists a different number of fields than this type has. "                \
		"List every field that goes on the wire, in declaration order."                             \
	);                                                                                              \
	static_assert(                                                                                  \
		!::ser::detail::HAS_INITIALIZER_LIST_CTOR_V<Self>,                                          \
		"ser: this type accepts a BRACED list of any length - a std::initializer_list "             \
		"constructor - so the braces SER_MAKE_FROM expands to would be swallowed as a "             \
		"list instead of calling the constructor with the fields as arguments. Use "                \
		"SER_MAKE_FROM_PAREN(Type, a, b) - it uses parentheses."                                    \
	);                                                                                              \
	static_assert(                                                                                  \
		::ser::detail::braced_from_fields<Self, FOR_EACH_COMMA(SER_DETAIL_MAKE_TYPE, __VA_ARGS__)>, \
		"ser: this type cannot be built from its fields, in braces, in the order "                  \
		"given. Check the order against the constructor - two fields of different "                 \
		"types swapped is exactly what this catches. If the order is right, the other "             \
		"cause is a narrowing conversion, which braces refuse and parentheses allow: "              \
		"SER_MAKE_FROM_PAREN(Type, a, b) accepts it, or widen the parameter."                       \
	);

#define SER_MAKE_FROM(Self, ...)                                         \
	static Self serMake(::ser::reader auto& ar) {                        \
		SER_DETAIL_MAKE_FROM_CHECKS(Self, __VA_ARGS__)                   \
		return Self{ FOR_EACH_COMMA(SER_DETAIL_MAKE_ARG, __VA_ARGS__) }; \
	}

// Same, without the field list: the fields come from the structured-bindings walk, so
// the type has to be enumerable. It cannot check the order of the constructor arguments
// against anything, which is why SER_TEST_ROUNDTRIP is a CONDITION of using it and not
// a suggestion.
#define SER_MAKE_FROM_MEMBERS(Self)                                                         \
	static Self serMake(::ser::reader auto& ar) {                                           \
		static_assert(                                                                      \
			::ser::CAN_ENUMERATE_MEMBERS_V<Self>,                                           \
			"ser: SER_MAKE_FROM_MEMBERS needs to know this type's fields. Declare "         \
			"using ser_members = ser::members<N>; or list them: SER_MAKE_FROM(Type, a, b)." \
		);                                                                                  \
		static_assert(                                                                      \
			!::ser::detail::HAS_INITIALIZER_LIST_CTOR_V<Self>,                              \
			"ser: this type accepts a BRACED list of any length - a std::initializer_list " \
			"constructor - and SER_MAKE_FROM_MEMBERS expands to braces exactly as "         \
			"SER_MAKE_FROM does, so the fields would be swallowed as a list instead of "    \
			"reaching the constructor. Use SER_MAKE_FROM_PAREN(Type, a, b) - it uses "      \
			"parentheses. SER_MAKE_FROM_MEMBERS_PAREN(Type) is the same thing without a "   \
			"field list, which this type does not need."                                    \
		);                                                                                  \
		return ::ser::detail::makeByFields<Self>(ar);                                       \
	}

// Fields from the walk, constructor through parentheses. The two reasons to reach for
// the other variants at once: nothing to list, and braces that would be swallowed.
#define SER_MAKE_FROM_MEMBERS_PAREN(Self)                                                         \
	static Self serMake(::ser::reader auto& ar) {                                                 \
		static_assert(                                                                            \
			::ser::CAN_ENUMERATE_MEMBERS_V<Self>,                                                 \
			"ser: SER_MAKE_FROM_MEMBERS_PAREN needs to know this type's fields. Declare "         \
			"using ser_members = ser::members<N>; or list them: SER_MAKE_FROM_PAREN(Type, a, b)." \
		);                                                                                        \
		return ::ser::detail::makeParenByFields<Self>(ar);                                        \
	}

// ── SER_DESCRIBE_MAKE ─────────────────────────────────────────────────────────
// The third of the three hook forms.
//
//   SER_DESCRIBE      - serVisit:            one function, both directions, and the
//                       object has to exist before it can be filled in.
//   SER_DESCRIBE_MAKE - serWrite + serMake: same field list, both directions, and the
//                       object is BUILT on the way in - const fields, no default
//                       constructor, no move constructor.
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
// ser_described, so the compiler reports a redefinition. The two are alternatives anyway
// - a type whose serVisit can fill it in does not need a serMake - and checkHooks refuses
// the pair on its own merits.
#define SER_DESCRIBE_MAKE(Self, ...)                                                  \
	SER_DETAIL_DESCRIBE_FIELDS(__VA_ARGS__)                                           \
	static constexpr ::ser::Errc serWrite(::ser::writer auto& ar, const Self& self) { \
		SER_DETAIL_FIELD_CHECKS_BUILT(__VA_ARGS__)                                    \
		return ar(FOR_EACH_COMMA(SER_DETAIL_QUALIFY, __VA_ARGS__));                   \
	}                                                                                 \
	SER_MAKE_FROM(Self, __VA_ARGS__)

// The same for a constructor that would swallow the braces - see SER_MAKE_FROM_PAREN.
// The write side is unaffected by that; only the read side changes.
#define SER_DESCRIBE_MAKE_PAREN(Self, ...)                                            \
	SER_DETAIL_DESCRIBE_FIELDS(__VA_ARGS__)                                           \
	static constexpr ::ser::Errc serWrite(::ser::writer auto& ar, const Self& self) { \
		SER_DETAIL_FIELD_CHECKS_BUILT(__VA_ARGS__)                                    \
		return ar(FOR_EACH_COMMA(SER_DETAIL_QUALIFY, __VA_ARGS__));                   \
	}                                                                                 \
	SER_MAKE_FROM_PAREN(Self, __VA_ARGS__)

// Parentheses instead of braces, for a constructor that would otherwise be shadowed by
// std::initializer_list. Parentheses do not order their arguments, so the fields are
// read into locals first - declarations, which ARE ordered - and moved in. That move is
// the price, and it is why this is not the default.
#define SER_DETAIL_MAKE_LOCAL(x) auto ser_local_##x = ::ser::readField<SER_DETAIL_MAKE_TYPE(x)>(ar);
#define SER_DETAIL_MOVE_LOCAL(x) ::std::move(ser_local_##x)

#define SER_MAKE_FROM_PAREN(Self, ...)                                                   \
	static Self serMake(::ser::reader auto& ar) {                                        \
		using ser_detail_self_t = Self;                                                  \
		static_assert(                                                                   \
			::ser::detail::makeFromArityOk<Self, SER_DETAIL_FIELD_COUNT(__VA_ARGS__)>(), \
			"ser: SER_MAKE_FROM_PAREN lists a different number of fields than this "     \
			"type has. List every field that goes on the wire, in declaration order."    \
		);                                                                               \
		FOR_EACH(SER_DETAIL_MAKE_LOCAL, __VA_ARGS__)                                     \
		return Self(FOR_EACH_COMMA(SER_DETAIL_MOVE_LOCAL, __VA_ARGS__));                 \
	}
