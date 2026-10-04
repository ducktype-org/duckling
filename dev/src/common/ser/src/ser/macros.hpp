#pragma once


/**
 * @file
 * @brief The `SER_*` macros a type writes by hand, reached through `<ser/ser.hpp>`.
 * @note Public and internal are told apart by NAME here, not by file: everything spelled
 * `SER_INTERNAL_*`, and everything in `namespace ser::internal`, is implementation. They stay
 * next to the public macro each one serves - a helper three files away from its only caller
 * is harder to read, not easier.
 *
 * Public: SER_DESCRIBE, SER_DESCRIBE_MAKE(_PAREN), SER_MAKE_FROM(_PAREN),
 *         SER_MAKE_FROM_MEMBERS(_PAREN). SER_FRIEND lives in <ser/access.hpp>.
 */

#include <base/comptime/aggregate_arity.hpp>
#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/preproc/for_each.hpp>
#include <base/preproc/stringify.hpp>

#include <ser/access.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/internal/describe.hpp>
#include <ser/internal/dispatch.hpp>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

/**
 * @brief FOR_EACH and FOR_EACH_COMMA come from base: statements and an argument list
 * respectively. The field count is an EXPRESSION rather than a token, which is all any
 * caller here needs it to be.
 */
#define SER_INTERNAL_PLUS_ONE(x) +1
#define SER_INTERNAL_FIELD_COUNT(...) \
	(::std::size_t{ 0 } FOR_EACH(SER_INTERNAL_PLUS_ONE, __VA_ARGS__))

#define SER_INTERNAL_QUALIFY(x) self.x

namespace ser::internal {

	/*
	 * what the SER_MAKE_FROM checks are made of
	 * Every one of them exists because getting it wrong is SILENT: a permuted argument list
	 * of the same types compiles and reads back as garbage, and a missing field compiles
	 * and produces a stream one field short.
	 */

	/**
	 * @brief The argument count has to match the field count, when it is knowable at all. For a
	 * type nobody can enumerate this is vacuously true and SER_TEST_ROUNDTRIP is the only
	 * thing left that can catch a mistake.
	 */
	template<class T, ::std::size_t N>
	consteval bool makeFromArityOk() {
		/**
		 * @brief A base class makes the two numbers mean different things: the declared count is
		 * about this class's own members, while the macro lists every field that goes on the
		 * wire, inherited ones included. Comparing them would refuse code that works.
		 */
		if constexpr (::base::HAS_BASE_V<T>)
			return true;
		else if constexpr (CAN_ENUMERATE_MEMBERS_V<T>)
			return N == MEMBER_COUNT_V<T>;
		else
			return true;
	}

	/**
	 * @brief Can the constructor be called with these fields, the way the macro will call it?
	 * std::is_constructible_v answers a different question twice over: it asks about
	 * PARENTHESES while the macro expands to BRACES, and it asks with an XVALUE while every
	 * argument the macro passes is a PRVALUE out of dispatchMake. So the question is asked
	 * in the shape it will be answered in - asPrvalue<F>() is a call expression and
	 * therefore a prvalue, and the braces are the macro's own.
	 */
	template<class F>
	F asPrvalue(); /* NOT defined, on purpose */

	template<class T, class... Fs>
	concept BracedFromFields = requires { T{ asPrvalue<Fs>()... }; };

	/**
	 * @brief A constructor taking std::initializer_list swallows braces: Type{a, b} would build a
	 * two-element container instead of calling the two-argument constructor. Such a type
	 * needs SER_MAKE_FROM_PAREN, and this tells it apart.
	 *
	 * Two routes, because neither is enough alone. Asking directly needs T::value_type,
	 * which a hand-written type need not have; so the second route asks whether a BRACED
	 * list of ANY length constructs the type - a normal type stops at its field count, a
	 * list never stops, and a variadic constructor template does not fool it because a
	 * braced-init-list is a non-deduced context.
	 */
	template<class T>
	consteval bool hasInitializerListCtor() {
		if constexpr (requires { typename T::value_type; })
			if (::std::is_constructible_v<T, ::std::initializer_list<typename T::value_type>>)
				return true;
		return ARITY_SATURATED_V<T>;
	}

	template<class T>
	inline constexpr bool HAS_INITIALIZER_LIST_CTOR_V = hasInitializerListCtor<T>();

} /* namespace ser::internal */

/**
 * @brief the two field shapes a hook has to refuse for itself
 * @details A type with a hook never reaches the automatic walk, so the walk's own field checks
 * never run for it - and by the time `ar(x, y)` runs the arguments are expressions, which carry
 * neither property. A MACRO can still see the field NAMES, which is what makes the question askable
 * at all.
 *
 * Both go through declval<Self&>() rather than `self`, because the write side takes self by
 * const reference and a const reference binds to a bit-field perfectly well.
 */
#define SER_INTERNAL_FIELD_REF_OK(x)                                                      \
	static_assert(                                                                        \
		!::std::is_reference_v<decltype(::std::declval<ser_internal_field_self_t&>().x)>, \
		"ser: cannot serialize a REFERENCE field. Reading has to overwrite the object "   \
		"and a reference can never be rebound. Hold the value itself, or leave this "     \
		"field out of the list."                                                          \
	);

#define SER_INTERNAL_FIELD_BITS_OK(x)                                                       \
	static_assert(                                                                          \
		requires { ::base::bindProbe(::std::declval<ser_internal_field_self_t&>().x); },    \
		"ser: cannot serialize a BIT-FIELD through serVisit. serVisit reads by writing "    \
		"through a reference to each field, and nothing binds a reference to a bit-field. " \
		"The automatic member walk handles a bit-field by BUILDING the object instead, so " \
		"there are three ways out: drop SER_DESCRIBE and let the walk do it, use "          \
		"SER_DESCRIBE_MAKE(Type, a, b) whose read side builds, or leave this field out of " \
		"the list. The wire widens a bit-field to its declared type either way."            \
	);

/** @brief The two halves together, for a hook that reads by writing through a reference. */
#define SER_INTERNAL_FIELD_OK(x) \
	SER_INTERNAL_FIELD_REF_OK(x) \
	SER_INTERNAL_FIELD_BITS_OK(x)

/**
 * @brief The write side of SER_DESCRIBE_MAKE needs only the reference half: its read side is
 * serMake, which BUILDS, and braces initialize a bit-field by value.
 */
#define SER_INTERNAL_FIELD_CHECKS_BUILT(...)                                 \
	using ser_internal_field_self_t = ::std::remove_cvref_t<decltype(self)>; \
	FOR_EACH(SER_INTERNAL_FIELD_REF_OK, __VA_ARGS__)

#define SER_INTERNAL_FIELD_CHECKS(...)                                       \
	using ser_internal_field_self_t = ::std::remove_cvref_t<decltype(self)>; \
	FOR_EACH(SER_INTERNAL_FIELD_OK, __VA_ARGS__)

/*
 * SER_DESCRIBE
 * Lists the fields that go on the wire, in order, and skips the rest. It expands to an
 * in-class serVisit, so dispatch finds it at level 2 and nothing else has to know about
 * it - and to the field names, which is what lets SER_TEST_ROUNDTRIP say which field came
 * back wrong instead of just "not equal". At least one field, and the macro signature is
 * what enforces it.
 */

/**
 * @brief The half that only DESCRIBES: the count, the names, and the fields as a tuple.
 * @details It says nothing about the format, so it sits under either of the two hook forms
 * below.
 *
 * std::array rather than a C array, and that is about the CALLER: clang-tidy reports
 * cppcoreguidelines-avoid-c-arrays at the line that USES the macro, so a C array here would
 * make every type that says SER_DESCRIBE carry a NOLINT.
 */
#define SER_INTERNAL_DESCRIBED_ELEM(x) \
	::std::conditional_t<requires {    \
		&self.x;                       \
	}, const ::std::remove_cvref_t<decltype(self.x)>&, ::std::remove_cvref_t<decltype(self.x)>>

#define SER_INTERNAL_DESCRIBE_FIELDS(...)                                                             \
	static constexpr ::std::size_t ser_field_count = SER_INTERNAL_FIELD_COUNT(__VA_ARGS__);           \
	static constexpr ::std::array<const char*, SER_INTERNAL_FIELD_COUNT(__VA_ARGS__)> ser_field_names \
		= { FOR_EACH_COMMA(STRINGIFY_2, __VA_ARGS__) };                                               \
	static constexpr auto ser_described(auto& self) {                                                 \
		return ::std::tuple<FOR_EACH_COMMA(SER_INTERNAL_DESCRIBED_ELEM, __VA_ARGS__)>{                \
			FOR_EACH_COMMA(SER_INTERNAL_QUALIFY, __VA_ARGS__)                                         \
		};                                                                                            \
	}


#define SER_DESCRIBE(F1, ...)                                                           \
	SER_INTERNAL_DESCRIBE_FIELDS(F1 __VA_OPT__(, ) __VA_ARGS__)                         \
	static constexpr ::ser::Errc serVisit(::ser::ReaderOrWriter auto& ar, auto& self) { \
		SER_INTERNAL_FIELD_CHECKS(F1 __VA_OPT__(, ) __VA_ARGS__)                        \
		return ar(FOR_EACH_COMMA(SER_INTERNAL_QUALIFY, F1 __VA_OPT__(, ) __VA_ARGS__)); \
	}

/**
 * @brief SER_MAKE_FROM
 * @details The hook for a type that cannot be filled in after the fact: const fields, no default
 * constructor, no move constructor. Names the constructor arguments in order.
 *
 *     class Point {
 *         const int x, y;
 *     public:
 *         Point(int a, int b) : x(a), y(b) {}
 *         SER_MAKE_FROM(Point, x, y)
 *         static ser::Errc serWrite(ser::Writer auto& ar, const Point& p) { ... }
 *     };
 *
 * No local variables anywhere in the expansion: each argument is a prvalue that
 * initializes its parameter directly, so nothing is ever moved.
 */
#define SER_INTERNAL_MAKE_TYPE(x) ::std::remove_cv_t<decltype(ser_internal_self_t::x)>
#define SER_INTERNAL_MAKE_ARG(x)  ::ser::subMake<SER_INTERNAL_MAKE_TYPE(x)>(ar)

#define SER_INTERNAL_MAKE_FROM_CHECKS(Self, ...)                                                      \
	using ser_internal_self_t = Self;                                                                 \
	static_assert(                                                                                    \
		::ser::internal::makeFromArityOk<Self, SER_INTERNAL_FIELD_COUNT(__VA_ARGS__)>(),              \
		"ser: SER_MAKE_FROM lists a different number of fields than this type has. "                  \
		"List every field that goes on the wire, in declaration order."                               \
	);                                                                                                \
	static_assert(                                                                                    \
		!::ser::internal::HAS_INITIALIZER_LIST_CTOR_V<Self>,                                          \
		"ser: this type accepts a BRACED list of any length - a std::initializer_list "               \
		"constructor - so the braces SER_MAKE_FROM expands to would be swallowed as a "               \
		"list instead of calling the constructor with the fields as arguments. Use "                  \
		"SER_MAKE_FROM_PAREN(Type, a, b) - it uses parentheses."                                      \
	);                                                                                                \
	static_assert(                                                                                    \
		::ser::internal::BracedFromFields<Self, FOR_EACH_COMMA(SER_INTERNAL_MAKE_TYPE, __VA_ARGS__)>, \
		"ser: this type cannot be built from its fields, in braces, in the order "                    \
		"given. Check the order against the constructor - two fields of different "                   \
		"types swapped is exactly what this catches. If the order is right, the other "               \
		"cause is a narrowing conversion, which braces refuse and parentheses allow: "                \
		"SER_MAKE_FROM_PAREN(Type, a, b) accepts it, or widen the parameter."                         \
	);

#define SER_MAKE_FROM(Self, ...)                                           \
	static Self serMake(::ser::Reader auto& ar) {                          \
		SER_INTERNAL_MAKE_FROM_CHECKS(Self, __VA_ARGS__)                   \
		return Self{ FOR_EACH_COMMA(SER_INTERNAL_MAKE_ARG, __VA_ARGS__) }; \
	}

/**
 * @brief Same, without the field list: the fields come from the structured-bindings walk, so
 * the type has to be enumerable. It cannot check the order of the constructor arguments
 * against anything, which is why SER_TEST_ROUNDTRIP is a CONDITION of using it and not
 * a suggestion.
 */
#define SER_MAKE_FROM_MEMBERS(Self)                                                         \
	static Self serMake(::ser::Reader auto& ar) {                                           \
		static_assert(                                                                      \
			::ser::CAN_ENUMERATE_MEMBERS_V<Self>,                                           \
			"ser: SER_MAKE_FROM_MEMBERS needs to know this type's fields. Declare "         \
			"using ser_members = ser::Members<N>; or list them: SER_MAKE_FROM(Type, a, b)." \
		);                                                                                  \
		static_assert(                                                                      \
			!::ser::internal::HAS_INITIALIZER_LIST_CTOR_V<Self>,                            \
			"ser: this type accepts a BRACED list of any length - a std::initializer_list " \
			"constructor - and SER_MAKE_FROM_MEMBERS expands to braces exactly as "         \
			"SER_MAKE_FROM does, so the fields would be swallowed as a list instead of "    \
			"reaching the constructor. Use SER_MAKE_FROM_PAREN(Type, a, b) - it uses "      \
			"parentheses. SER_MAKE_FROM_MEMBERS_PAREN(Type) is the same thing without a "   \
			"field list, which this type does not need."                                    \
		);                                                                                  \
		return ::ser::internal::makeByFields<Self>(ar);                                     \
	}

/**
 * @brief Fields from the walk, constructor through parentheses. The two reasons to reach for
 * the other variants at once: nothing to list, and braces that would be swallowed.
 */
#define SER_MAKE_FROM_MEMBERS_PAREN(Self)                                                         \
	static Self serMake(::ser::Reader auto& ar) {                                                 \
		static_assert(                                                                            \
			::ser::CAN_ENUMERATE_MEMBERS_V<Self>,                                                 \
			"ser: SER_MAKE_FROM_MEMBERS_PAREN needs to know this type's fields. Declare "         \
			"using ser_members = ser::Members<N>; or list them: SER_MAKE_FROM_PAREN(Type, a, b)." \
		);                                                                                        \
		return ::ser::internal::makeParenByFields<Self>(ar);                                      \
	}

/**
 * @brief SER_DESCRIBE_MAKE
 * @details The third of the three hook forms.
 *
 *   SER_DESCRIBE      - serVisit:            one function, both directions, and the
 *                       object has to exist before it can be filled in.
 *   SER_DESCRIBE_MAKE - serWrite + serMake: same field list, both directions, and the
 *                       object is BUILT on the way in - const fields, no default
 *                       constructor, no move constructor.
 *
 *     class Reading {
 *         const std::uint32_t sensor;
 *         float value;
 *     public:
 *         Reading(std::uint32_t s, float v) : sensor(s), value(v) {}
 *         SER_FRIEND
 *     private:
 *         SER_DESCRIBE_MAKE(Reading, sensor, value)
 *     };
 *
 * Not to be combined with SER_DESCRIBE on one type: both define ser_field_names and
 * ser_described, so the compiler reports a redefinition. They are alternatives anyway, and
 * checkHooks refuses the pair on its own merits.
 */
#define SER_DESCRIBE_MAKE(Self, F1, ...)                                                \
	SER_INTERNAL_DESCRIBE_FIELDS(F1 __VA_OPT__(, ) __VA_ARGS__)                         \
	static constexpr ::ser::Errc serWrite(::ser::Writer auto& ar, const Self& self) {   \
		SER_INTERNAL_FIELD_CHECKS_BUILT(F1 __VA_OPT__(, ) __VA_ARGS__)                  \
		return ar(FOR_EACH_COMMA(SER_INTERNAL_QUALIFY, F1 __VA_OPT__(, ) __VA_ARGS__)); \
	}                                                                                   \
	SER_MAKE_FROM(Self, F1 __VA_OPT__(, ) __VA_ARGS__)

/**
 * @brief The same for a constructor that would swallow the braces - see SER_MAKE_FROM_PAREN.
 * The write side is unaffected by that; only the read side changes.
 */
#define SER_DESCRIBE_MAKE_PAREN(Self, F1, ...)                                          \
	SER_INTERNAL_DESCRIBE_FIELDS(F1 __VA_OPT__(, ) __VA_ARGS__)                         \
	static constexpr ::ser::Errc serWrite(::ser::Writer auto& ar, const Self& self) {   \
		SER_INTERNAL_FIELD_CHECKS_BUILT(F1 __VA_OPT__(, ) __VA_ARGS__)                  \
		return ar(FOR_EACH_COMMA(SER_INTERNAL_QUALIFY, F1 __VA_OPT__(, ) __VA_ARGS__)); \
	}                                                                                   \
	SER_MAKE_FROM_PAREN(Self, F1 __VA_OPT__(, ) __VA_ARGS__)

/**
 * @brief Parentheses instead of braces, for a constructor that would otherwise be shadowed by
 * std::initializer_list. Parentheses do not order their arguments, so the fields are
 * read into locals first - declarations, which ARE ordered - and moved in. That move is
 * the price, and it is why this is not the default.
 */
#define SER_INTERNAL_MAKE_LOCAL(x) \
	auto ser_local_##x = ::ser::subMake<SER_INTERNAL_MAKE_TYPE(x)>(ar);
#define SER_INTERNAL_MOVE_LOCAL(x) ::std::move(ser_local_##x)

#define SER_MAKE_FROM_PAREN(Self, ...)                                                       \
	static Self serMake(::ser::Reader auto& ar) {                                            \
		using ser_internal_self_t = Self;                                                    \
		static_assert(                                                                       \
			::ser::internal::makeFromArityOk<Self, SER_INTERNAL_FIELD_COUNT(__VA_ARGS__)>(), \
			"ser: SER_MAKE_FROM_PAREN lists a different number of fields than this "         \
			"type has. List every field that goes on the wire, in declaration order."        \
		);                                                                                   \
		FOR_EACH(SER_INTERNAL_MAKE_LOCAL, __VA_ARGS__)                                       \
		return Self(FOR_EACH_COMMA(SER_INTERNAL_MOVE_LOCAL, __VA_ARGS__));                   \
	}
