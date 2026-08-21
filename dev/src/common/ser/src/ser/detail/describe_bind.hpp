#pragma once

#include <ser/access.hpp>
#include <ser/config.hpp>
#include <ser/detail/ladder.hpp>
#include <ser/detail/meta.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser::detail {

	inline constexpr ::std::size_t NO_COUNT = static_cast<::std::size_t>(-1);

	// Taken from the ladder itself, so the two cannot drift apart.
	inline constexpr ::std::size_t MAX_MEMBERS = LADDER_MAX;

	// ── the counting probe ────────────────────────────────────────────────────
	// Converts to anything and is deliberately never defined: it appears only inside a
	// requires-expression, where the initialization is never evaluated.
	//
	// Clang reports -Wundefined-inline for it because it counts being CHOSEN by overload
	// resolution as being used. Defining it would mean returning a T that cannot be built,
	// for every T, so the declaration stays and the warning is silenced exactly here.
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
	struct any_init {
		template<class T>
		constexpr operator T() const;  // NOT defined, on purpose
	};
#if defined(__clang__)
	#pragma clang diagnostic pop
#endif

	// ── one probe, and why one is enough ──────────────────────────────────────
	// T{ p, p, p } - the largest N that compiles is the field count. Each clause
	// COPY-INITIALIZES its member, and copy-initialization is the context where a
	// conversion function is a first-class path: reaching a member of class type M uses
	// `operator M` directly and never touches M's own constructors. So the probe reaches a
	// member whatever its constructors look like - explicit, multi-argument, or absent.
	//
	// A clause in BRACES would be copy-LIST-initialization, where the same route has to go
	// through M's copy constructor - and whether it is available, and whether it then ties
	// with M's own converting constructor, is where compilers part company. So there is one
	// probe, unbraced.
	//
	// THE PRICE IS C ARRAYS, and it is paid deliberately. An unbraced clause is subject to
	// brace elision, and elision happens exactly where the member cannot be copy-initialized
	// from the clause. any_init converts to anything, so a scalar, a class or a nested
	// aggregate never elides - but nothing converts to `int[3]`, so an array field consumes
	// one clause PER ELEMENT and the count comes out too large. A C array field in an
	// automatically counted aggregate is therefore not supported; because the arity of a
	// structured binding is CHECKED against the type, that can only ever surface as a
	// compile error and never as wrong bytes. `using ser_members = ser::members<N>;` skips
	// the probe, and such a type then walks and serializes exactly as any other.
	template<class T, ::std::size_t... I>
	consteval bool initWith() {
		return requires { T{ (void(I), any_init{})... }; };
	}

	template<class T, ::std::size_t... I>
	consteval bool initWithSeq(::std::index_sequence<I...>) {
		return initWith<T, I...>();
	}

	// ── the scan ──────────────────────────────────────────────────────────────
	// Which clause counts are valid forms ONE CONTIGUOUS RUN. Below it a member without a
	// default constructor is left with no initializer; above it there are simply too many
	// clauses. So the largest valid N is found by walking up and stopping one past the last
	// success, and Best carries that success along as a template argument because only a
	// template argument can drive the `if constexpr` that stops.
	//
	// A recursion and not a fold: a fold instantiates the probe once per point of the whole
	// range for every type, and MSVC gives up at roughly a thousand instantiations of it in
	// one translation unit. The scan spends about as many as the type has fields.
	template<class T, ::std::size_t N, ::std::size_t Best>
	consteval ::std::size_t scanArity() {
		if constexpr (N > MAX_MEMBERS + 1)
			return Best;
		else if constexpr (initWithSeq<T>(::std::make_index_sequence<N>{}))
			return scanArity<T, N + 1, N>();         // keep going
		else if constexpr (Best != NO_COUNT)
			return Best;                             // one past the top
		else
			return scanArity<T, N + 1, NO_COUNT>();  // nothing has fit yet
	}

	// Probed one PAST the ladder's limit on purpose. A type whose braces accept any number
	// of clauses - a std::initializer_list constructor - never stops, and reporting
	// MAX_MEMBERS + 1 tells it apart from a type that genuinely has MAX_MEMBERS fields.
	template<class T>
	inline constexpr ::std::size_t ELIDED_ARITY_V = scanArity<T, 0, NO_COUNT>();

	// ── "are these braces a LIST?" ────────────────────────────────────────────
	// Not part of counting - the probe above never puts a clause in braces. This is for
	// SER_MAKE_FROM, which expands to braces and has to refuse a type whose braces reach an
	// initializer_list constructor instead of the fields. A braced list one clause past the
	// ladder's limit answers it: a constructor stops at its own arity, an initializer_list
	// constructor never stops, and a variadic constructor template does not fool it because
	// a braced-init-list is a non-deduced context.
	//
	// It costs MAX_MEMBERS + 1 clauses, so it is asked only about the types SER_MAKE_FROM is
	// written on - never once per field of every counted aggregate.
	template<class T, ::std::size_t... I>
	consteval bool listInitWith() {
		return requires { T{ { (void(I), any_init{}) }... }; };
	}

	template<class T, ::std::size_t... I>
	consteval bool listInitWithSeq(::std::index_sequence<I...>) {
		return listInitWith<T, I...>();
	}

	template<class T>
	inline constexpr bool ACCEPTS_ANY_LENGTH_V
		= listInitWithSeq<T>(::std::make_index_sequence<MAX_MEMBERS + 1>{});

	// "Braces here do not mean fields."
	template<class T>
	inline constexpr bool ARITY_SATURATED_V = ACCEPTS_ANY_LENGTH_V<T>;

	// ── does this type have a base class? ─────────────────────────────────────
	// Aggregate initialization treats a base class as an ELEMENT - D{ B{1}, 2 } - while a
	// structured binding sees only data members, and refuses outright when the base has any
	// of its own. So a base makes the probed count wrong, silently, in a type that is
	// otherwise a perfectly ordinary aggregate.
	//
	// Detected the way Boost.PFR does it: a probe that converts to nothing except a base
	// of T. If the first element accepts it, the first element is a base.
	template<class T>
	struct base_init {
		template<class U>
		requires(!::std::is_same_v<U, T> && ::std::is_base_of_v<U, T>)
		constexpr operator U() const;  // NOT defined, on purpose
	};

	template<class T>
	inline constexpr bool HAS_BASE_V = requires { T{ base_init<T>{} }; };

	// ── can we enumerate this type's fields? ──────────────────────────────────
	// Asymmetric on purpose: false means "I do not know", never "it has no fields". Every
	// caller treats false as "ask the user", so a wrong false costs a message and a wrong
	// true costs a format.
	//
	// The branches are `if constexpr` and not a conjunction: `&&` instantiates both
	// operands, and the cheap structural answers have to come first.
	template<class T>
	consteval bool canEnumerateImpl() {
		using U = ::std::remove_cv_t<T>;

		if constexpr (access::NAMES_MEMBER_COUNT_V<U>)
			return true;  // the author said so
		else if constexpr (::std::is_union_v<U> || ::std::is_array_v<U> || !::std::is_class_v<U>)
			return false;
		else if constexpr (!::std::is_aggregate_v<U>)
			return false;  // cannot probe a non-aggregate
		// No check for a polymorphic type: a class with virtual functions is never an
		// aggregate, so the line above has already refused it.
		else if constexpr (HAS_BASE_V<U>)
			return false;  // the probe would count the base
		else if constexpr (::std::is_empty_v<U>)
			return true;   // zero fields, and that is known
		else
			// Zero is not a count for a type that has fields - it means only the empty
			// clause list compiled - and a number past the ladder's limit has no rung
			// that could walk it.
			return ELIDED_ARITY_V<U> != NO_COUNT && ELIDED_ARITY_V<U> != 0
			    && ELIDED_ARITY_V<U> <= MAX_MEMBERS;
	}

}  // namespace ser::detail

namespace ser {

	// ── the public field-count surface ────────────────────────────────────────
	template<class T>
	inline constexpr bool CAN_ENUMERATE_MEMBERS_V
		= detail::canEnumerateImpl<::std::remove_cv_t<T>>();

	namespace detail {
		template<class T>
		consteval ::std::size_t memberCountOf() {
			if constexpr (access::NAMES_MEMBER_COUNT_V<T>)
				return access::declaredMemberCount<T>();
			else if constexpr (::std::is_empty_v<T>)
				return 0;
			else
				return ELIDED_ARITY_V<T>;
		}
	}

	// Only ask this when CAN_ENUMERATE_MEMBERS_V<T> is true. It is a plain value rather
	// than a hard error otherwise, because dispatch needs to test the guard first.
	template<class T>
	inline constexpr ::std::size_t MEMBER_COUNT_V = detail::memberCountOf<::std::remove_cv_t<T>>();

	// ── visiting ──────────────────────────────────────────────────────────────
	// The arity lives here rather than in ser::access because the count needs the ladder
	// (member_type_t below is built out of it) and the ladder must therefore need
	// nothing. access::visitMembersN takes N explicitly for exactly that reason.
	template<class T, class F>
	constexpr decltype(auto) visitMembers(T&& obj, F&& f) {
		using U = ::std::remove_cvref_t<T>;
		// A declared count is NOT compared against the probe, because the probe can err in
		// both directions: a C array field overshoots (one clause per element), and the scan
		// undershoots on a member no clause can initialize, a non-const reference being
		// exactly that. So neither direction is evidence of a wrong declaration. A wrong
		// ser::members<N> is caught where it cannot be wrong about anything: the arity of a
		// structured binding is CHECKED against the type, so it is a compile error in the
		// rung it selects.
		static_assert(
			CAN_ENUMERATE_MEMBERS_V<U>,
			"ser: cannot enumerate this type's fields. Declare the count in the class:\n"
			"  using ser_members = ser::members<N>;   (with SER_FRIEND for private fields)\n"
			"or give the type a hook: serVisit, or serWrite + serRead.\n"
			"A C array field is the common reason: it elides braces, so the count comes "
			"out too large and ser::members<N> is what settles it."
		);
		return access::visitMembersN<MEMBER_COUNT_V<U>>(
			::std::forward<T>(obj), ::std::forward<F>(f)
		);
	}

	// ── member_type_t ─────────────────────────────────────────────────────────
	// The Boost.PFR trick: tie the members into a tuple of references and read the types
	// back off it. Never called - only its return type is ever needed - so the
	// declval<T&> is harmless even for a type that cannot be created.
	template<class T>
	constexpr auto tieMembers(T& obj) {
		return access::visitMembersN<MEMBER_COUNT_V<T>>(obj, [](auto&... fields) {
			return ::std::tie(fields...);
		});
	}

	template<class T>
	using member_tuple_t = decltype(tieMembers(::std::declval<::std::remove_cv_t<T>&>()));

	// The stripping here is why field_decls_t below exists at all. A field's DECLARED type
	// survives only as long as the binding does: handing a binding to a parameter of type
	// `auto&` turns both a `const int&` member and a `const int` member into `const int&`,
	// because an expression never has reference type. So no guard downstream of the walk
	// can tell a reference field from a const one, and one of those two is a shape that
	// works - which is why the guards ask field_decls_t and the serializing paths do not.
	namespace detail {
		// field_decls_t -> the same fields with cv and references stripped, in both the
		// shapes the rest of the library asks for.
		template<class L>
		struct decl_field_types;

		template<class... Ds>
		struct decl_field_types<type_list<Ds...>> {
			using tuple = ::std::tuple<::std::remove_cvref_t<typename Ds::type>...>;
			using list  = type_list<::std::remove_cvref_t<typename Ds::type>...>;
		};
	}

	// The same fields as field_types_t, but as they were DECLARED - `int&` for a
	// reference member, `const int` for a const one, plus whether a non-const lvalue
	// reference can bind to each.
	//
	// Only the guards use this. Every serializing path keeps using field_types_t, which
	// must stay stripped: a `const int` field has to dispatch as `int`.
	template<class T>
	using field_decls_t = decltype(access::fieldDeclsN<MEMBER_COUNT_V<::std::remove_cv_t<T>>>(
		::std::declval<::std::remove_cv_t<T>&>()
	));

	// Taken off field_decls_t and not off member_tuple_t, and that is not a cosmetic
	// choice: member_tuple_t goes through std::tie, which needs a non-const reference per
	// field, and nothing binds one to a BIT-FIELD - so member_tuple_t<T> does not compile
	// at all for such a type. field_decls_t asks decltype on the binding instead, which
	// works for every field there is, and the answers agree everywhere both are available.
	//
	// It matters because the BUILD path needs these types, and the build path is exactly
	// what reads a type the walk cannot fill - a bit-field being the case.
	template<class T, ::std::size_t I>
	using member_type_t
		= ::std::tuple_element_t<I, typename detail::decl_field_types<field_decls_t<T>>::tuple>;

	// The same types as a pack rather than one index at a time - what a caller needs when
	// it wants every field type at once, and what dispatch hands to the build path.
	template<class T>
	using field_types_t = typename detail::decl_field_types<field_decls_t<T>>::list;

}  // namespace ser
