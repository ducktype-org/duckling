#pragma once

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>

#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/pointer_deny.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/internal/access_detect.hpp>
#include <ser/internal/adl.hpp>
#include <ser/internal/describe.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/serializer.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser::internal {

	/**
	 * @brief what the walk refuses to take apart
	 * @details A field shape no other guard here can see, because it is a property of the
	 * DECLARATION and an expression never carries it - FieldDeclsT is the one thing that
	 * still knows. Asked only where the walk is actually chosen, so a type with a hook
	 * never has its fields judged.
	 */
	template<class... Ds>
	consteval void checkFieldDecls(::base::TypeList<Ds...>) {
		static_assert(
			!(::std::is_reference_v<typename Ds::type> || ... || false),
			"ser: cannot serialize a REFERENCE field. Reading has to overwrite the object "
			"and a reference can never be rebound, so there is no honest thing to do with "
			"one: built out of fresh values it would point at a temporary that dies "
			"immediately, and filled in place it would silently overwrite whatever it "
			"already pointed at. Hold the value itself, or list the fields with "
			"SER_DESCRIBE(a, b) and leave the reference out."
		);
	}

	template<class T>
	consteval void checkWalkableFields() {
		checkFieldDecls(FieldDeclsT<T>{});
	}

	/**
	 * @brief fill, or build?
	 * @details The walk never assigns the object, it assigns the LEAVES, so what it needs is
	 * per-field: every field reachable by a writable reference, which rules out a const
	 * field and a bit-field. Whole-object assignability is asked in exactly one place,
	 * step 9.
	 */
	template<class T, class Ar>
	consteval bool readableInPlace();
	template<class T, class Ar>
	consteval bool walkCanFill();

	template<class Ar, class... Ds>
	consteval bool walkCanFillFields(::base::TypeList<Ds...>) {
		return (!::std::is_const_v<typename Ds::type> && ... && true)
		    && (Ds::BINDABLE && ... && true)
		    && (readableInPlace<::std::remove_cvref_t<typename Ds::type>, Ar>() && ... && true);
	}

	template<class T, class Ar>
	consteval bool walkCanFill() {
		if constexpr (!CAN_ENUMERATE_MEMBERS_V<T>)
			return false;  // the walk is not the way
		else
			return walkCanFillFields<Ar>(FieldDeclsT<T>{});
	}

	template<class T, class Ar>
	inline constexpr bool WALK_CAN_FILL_V = walkCanFill<T, Ar>();

	/**
	 * @brief "Can dispatchRead put a value into an object of this type that already exists?"
	 * An if-constexpr chain rather than a conjunction on purpose: a type with a hook must
	 * not have its fields inspected at all, because a hook is allowed to be the only thing
	 * that understands them.
	 */
	template<class T, class Ar>
	consteval bool readableInPlace() {
		if constexpr (::std::is_move_assignable_v<T>)
			return true;
		else if constexpr (HAS_ANY_READ_HOOK_V<T, Ar>)
			return true;  // the hook does it
		else if constexpr (builtin::DENIED_V<T>)
			return true;  // refused elsewhere
		else if constexpr (builtin::ScalarLike<T>)
			return true;
		else if constexpr (builtin::EnumLike<T>)
			return true;
		else if constexpr (!CAN_ENUMERATE_MEMBERS_V<T>)
			return true;                  // not this trait's call
		else
			return walkCanFill<T, Ar>();  // step 8, or nothing
	}

	/**
	 * @brief Step 9's question: the object exists and the walk cannot write into it, so build a
	 * fresh one and assign. A type that offers only serMake is the other caller.
	 */
	template<class T, class Ar>
	consteval bool readByBuilding() {
		if constexpr (HAS_ANY_MAKE_HOOK_V<T, Ar>)
			return ::std::is_move_assignable_v<T>;
		else if constexpr (!CAN_ENUMERATE_MEMBERS_V<T>)
			return false;
		else if constexpr (walkCanFill<T, Ar>())
			return false;  // step 8 has it
		else
			return ::std::is_move_assignable_v<T>;
	}

	template<class T, class Ar>
	inline constexpr bool READ_BY_BUILDING_V = readByBuilding<T, Ar>();

	/**
	 * @brief Step 4's question. Anything a hook or a builtin rule reads is left alone: the walk
	 * is not what will read it, so its fields are none of this trait's business.
	 */
	template<class T, class Ar>
	consteval bool mustBeBuilt() {
		if constexpr (HAS_ANY_READ_HOOK_V<T, Ar>)
			return false;
		else if constexpr (builtin::DENIED_V<T>)
			return false;
		else if constexpr (builtin::ScalarLike<T>)
			return false;
		else if constexpr (builtin::EnumLike<T>)
			return false;
		else if constexpr (!CAN_ENUMERATE_MEMBERS_V<T>)
			return false;
		else
			return !walkCanFill<T, Ar>();
	}

	template<class T, class Ar>
	inline constexpr bool MUST_BE_BUILT_V = mustBeBuilt<T, Ar>();

	/**
	 * @brief depth guard
	 * @details Data-dependent recursion (struct Tree { std::vector<Tree> kids; }) is an attack
	 * vector on read and a stack overflow on write.
	 */
	template<class Ar>
	class DepthGuard final {
		Ar&  ar;
		bool held;

	public:
		explicit constexpr DepthGuard(Ar& a) noexcept: ar(a), held(a.pushDepth()) {}

		constexpr ~DepthGuard() {
			if (held) ar.popDepth();
		}

		DepthGuard(const DepthGuard&)            = delete;
		DepthGuard& operator=(const DepthGuard&) = delete;

		[[nodiscard]] constexpr bool entered() const noexcept { return held; }
	};

	/**
	 * @brief the three dispatch contexts
	 * @details
	 *   dispatchWrite   object exists, produce bytes
	 *   dispatchRead    object exists, fill it from bytes
	 *   dispatchMake    no object yet, build and return one
	 *
	 * The order of the ladder is the whole design: a user hook outranks a builtin rule, a
	 * builtin rule outranks guessing, and within a level the asymmetric form outranks the
	 * symmetric one. Levels run trait, then in-class, then ADL - ser::Serializer<T> has to
	 * win, being the only one available for a type you cannot edit.
	 *
	 * Two rules that look like details and are not: a type with a hook is NEVER decomposed,
	 * and the refusals sit above every hook, because a pointer with a serializer is still a
	 * pointer.
	 */

	template<class T, Writer Ar>
	constexpr Errc dispatchWrite(Ar& ar, const T& x) {
		using U = ::std::remove_cv_t<T>;
		checkHooks<U, Ar>();

		DepthGuard g{ ar };
		if (!g.entered()) return Errc::DepthExceeded;

		if constexpr (builtin::DENIED_V<U>) {
			builtin::deny<U>();
			return Errc::InvalidValue;
		}

		else if constexpr (HAS_WRITE_V<Access::TraitHooks, U, Ar>)         // 1
			return Serializer<U>::write(ar, x);
		else if constexpr (HAS_VISIT_V<Access::TraitHooks, const U, Ar>)   // 2
			return Serializer<U>::visit(ar, x);
		else if constexpr (HAS_WRITE_V<Access::MemberHooks, U, Ar>)        // 3
			return Access::callWrite<U>(ar, x);
		else if constexpr (HAS_VISIT_V<Access::MemberHooks, const U, Ar>)  // 4
			return Access::callVisit(ar, x);
		else if constexpr (HAS_WRITE_V<AdlHooks, U, Ar>)                   // 5
			return adl_barrier::callWrite(ar, x);
		else if constexpr (HAS_VISIT_V<AdlHooks, const U, Ar>)             // 6
			return adl_barrier::callVisit(ar, x);

		else if constexpr (builtin::ScalarLike<U>)
			return builtin::writeScalar<U>(ar, x);  // 7
		else if constexpr (builtin::EnumLike<U>)
			return builtin::writeEnum<U>(ar, x);

		// Step 8. No hook, no builtin rule: walk the fields. It is last among the paths
		// that DO something, so a hook always outranks it - which is what keeps a
		// container-shaped type with serVisit from being taken apart field by field.
		else if constexpr (CAN_ENUMERATE_MEMBERS_V<U>) {  // 8
			checkWalkableFields<U>();
			return visitMembers(x, [&ar](const auto&... fields) { return ar(fields...); });
		}

		// checkHooks has already reported the hook this archive cannot call; the
		// generic message below would only point away from it.
		else if constexpr (::std::is_aggregate_v<U> && !CAN_ENUMERATE_MEMBERS_V<U>)
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: this type is an aggregate whose field count cannot be worked out. "
				"Two things cause it. A C ARRAY field elides braces, so counting sees its "
				"elements rather than the field. A BASE CLASS is an element of aggregate "
				"initialization but not of a structured binding, so counting sees one "
				"field too many. Declare the count in the class:\n"
				"  using ser_members = ser::Members<N>;\n"
				"That settles the array, and a base with no data members of its own. A base "
				"that HAS data members cannot be decomposed at all - give the type a "
				"serVisit hook."
			);

		else if constexpr (NONGENERIC_ANY_HOOK_V<U, Ar>)
			return Errc::InvalidValue;
		else if constexpr (builtin::looksPointerLike<U>()) {  // 9
			builtin::denyPointerLike<U>();
			return Errc::InvalidValue;
		} else
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: no way to serialize this type.\n"
				"  Aggregate with public fields?  nothing needed\n"
				"  Private fields?                using ser_members = ser::Members<N>; + "
				"SER_FRIEND\n"
				"  Has a constructor?             SER_MAKE_FROM(Type, a, b, c);\n"
				"  Not your type?                 specialize ser::Serializer<T>"
			);
	}

	template<class F>
	constexpr Errc codeFromHook(F&& f) {
		try {
			return ::std::forward<F>(f)();
		} catch (const Exception& e) { return e.code(); }
	}

	template<class T, Reader Ar>
	constexpr Errc dispatchRead(Ar& ar, T& x) {
		using U = ::std::remove_cv_t<T>;
		// Reachable from the member walk even though ser::In::operator() has its own
		// requires clause: a const FIELD gets here with the enclosing object perfectly
		// non-const.
		static_assert(
			!::std::is_const_v<T>,
			"ser: cannot read into a const object - reading overwrites it.\n"
			"  A whole object?  auto r = ser::read<T>(bytes);  and bind r to const\n"
			"  A const field?   SER_MAKE_FROM(Type, a, b, c); - a const field must be\n"
			"                   initialized at construction, it can never be assigned"
		);

		checkHooks<U, Ar>();

		DepthGuard g{ ar };
		if (!g.entered()) return Errc::DepthExceeded;

		if constexpr (builtin::DENIED_V<U>) {
			builtin::deny<U>();
			return Errc::InvalidValue;
		}

		else if constexpr (HAS_READ_V<Access::TraitHooks, U, Ar>)    // 1
			return codeFromHook([&] { return Serializer<U>::read(ar, x); });
		else if constexpr (HAS_VISIT_V<Access::TraitHooks, U, Ar>)   // 2
			return codeFromHook([&] { return Serializer<U>::visit(ar, x); });
		else if constexpr (HAS_READ_V<Access::MemberHooks, U, Ar>)   // 3
			return codeFromHook([&] { return Access::callRead<U>(ar, x); });
		else if constexpr (HAS_VISIT_V<Access::MemberHooks, U, Ar>)  // 4
			return codeFromHook([&] { return Access::callVisit(ar, x); });
		else if constexpr (HAS_READ_V<AdlHooks, U, Ar>)              // 5
			return codeFromHook([&] { return adl_barrier::callRead(ar, x); });
		else if constexpr (HAS_VISIT_V<AdlHooks, U, Ar>)             // 6
			return codeFromHook([&] { return adl_barrier::callVisit(ar, x); });

		else if constexpr (builtin::ScalarLike<U>)
			return builtin::readScalar<U>(ar, x);  // 7
		else if constexpr (builtin::EnumLike<U>)
			return builtin::readEnum<U>(ar, x);

		// Step 8. The walk must not take a type whose author wrote serMake
		else if constexpr (!HAS_ANY_MAKE_HOOK_V<U, Ar> && WALK_CAN_FILL_V<U, Ar>) {
			checkWalkableFields<U>();
			return visitMembers(x, [&ar](auto&... fields) { return ar(fields...); });
		}

		// Step 9. The object exists and cannot be written into field by field, so the only
		// way left is to build a fresh one and assign it - either a type offering only
		// serMake, or one the walk cannot fill (a BIT-FIELD field being that case). It
		// costs one assignment, which is why a type that can offer serRead should.
		else if constexpr (READ_BY_BUILDING_V<U, Ar>) {
			// dispatchMake signals by throwing; this context returns a code. The boundary
			// between the two conventions is here and nowhere else.
			try {
				x = dispatchMake<U>(ar);
				return Errc::Ok;
			} catch (const Exception& e) { return e.code(); }
		}

		else if constexpr (HAS_ANY_MAKE_HOOK_V<U, Ar>)
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,  // 10
				"ser: this type has serMake but no serRead, and it is not move-assignable, "
				"so an existing object cannot be filled in. Add serRead(ar, x), or make the "
				"type move-assignable."
			);

		// The same dead end reached from the other side: the walk cannot write into this
		// type's fields and it cannot be built-and-assigned either. A CONST field is what
		// produces that pair, because const is exactly what makes a type non-assignable.
		// Such a type is still readable on its own - ser::read<T>(bytes) builds it - just
		// not as a field of an object that is being filled.
		else if constexpr (MUST_BE_BUILT_V<U, Ar>)
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: this type cannot be filled in place - a const field, most likely - and "
				"it is not move-assignable either, so it cannot be built and assigned. On its "
				"own it still reads: ser::read<T>(bytes) builds it. As a FIELD of another type "
				"it needs one of - drop the const, add a serRead hook, or give the ENCLOSING "
				"type a serMake so this one is built rather than filled."
			);

		else if constexpr (::std::is_aggregate_v<U> && !CAN_ENUMERATE_MEMBERS_V<U>)
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: this type is an aggregate whose field count cannot be worked out. "
				"Two things cause it. A C ARRAY field elides braces, so counting sees its "
				"elements rather than the field. A BASE CLASS is an element of aggregate "
				"initialization but not of a structured binding, so counting sees one "
				"field too many. Declare the count in the class:\n"
				"  using ser_members = ser::Members<N>;\n"
				"That settles the array, and a base with no data members of its own. A base "
				"that HAS data members cannot be decomposed at all - give the type a "
				"serVisit hook."
			);

		else if constexpr (NONGENERIC_ANY_HOOK_V<U, Ar>)
			return Errc::InvalidValue;
		else if constexpr (builtin::looksPointerLike<U>()) {
			builtin::denyPointerLike<U>();
			return Errc::InvalidValue;
		} else
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: no way to deserialize this type.\n"
				"  Aggregate with public fields?  nothing needed\n"
				"  Private fields?                using ser_members = ser::Members<N>; + "
				"SER_FRIEND\n"
				"  Has a constructor?             SER_MAKE_FROM(Type, a, b, c);\n"
				"  Not your type?                 specialize ser::Serializer<T>"
			);
	}

	// building an object out of the stream, field by field
	// BRACES, never parentheses: [dcl.init.list]/4 orders the initializer clauses left to
	// right, so field 0 reads the first bytes of the record, while parentheses leave the
	// order unspecified. Every clause is a prvalue straight out of dispatchMake, so each
	// field is built once in its final storage - const fields, fields with no default
	// constructor and fields that cannot be moved all work.

	/**
	 * @brief one field is not always one clause
	 * @details A C array field cannot take a clause of its own - no function returns an array - but
	 * it does accept one clause per element, through brace elision. So an array field of
	 * extent N contributes N clauses, recursively, and every other field contributes one.
	 * The bytes are unchanged: the write side walks the same array in the same order.
	 */
	template<class F>
	struct FlattenField final {
		using type = ::base::TypeList<F>;
	};

	template<class E, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct FlattenField<E[N]> {
		using type = ::base::RepeatListT<typename FlattenField<E>::type, N>;
	};

	template<class T, class Seq>
	struct FlatFields;

	template<class T, ::std::size_t... I>
	struct FlatFields<T, ::std::index_sequence<I...>> {
		using type = ::base::CatListsT<typename FlattenField<MemberTypeT<T, I>>::type...>;
	};

	template<class T>
	using FlatFieldsT = typename FlatFields<T, ::std::make_index_sequence<MEMBER_COUNT_V<T>>>::type;

	// Two warnings are wrong about this function, and both are suppressed narrowly.
	//
	// Wmissing-braces is backwards: brace elision filling an array field one clause per
	// element is the mechanism here, and adding the braces would be the bug.
	//
	// Wconversion fires on a BIT-FIELD initialized from a prvalue of its own DECLARED type,
	// so the clause narrows to the width. It cannot lose anything - the write side read that
	// same bit-field and widened it - and there is nothing to cast to, a width not being a
	// type.
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wmissing-braces"
	#pragma clang diagnostic ignored "-Wconversion"
#elif defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wconversion"
#endif
	template<class T, class... Fs>
	constexpr T makeFromClauses(Reader auto& ar, ::base::TypeList<Fs...>) {
		return T{ dispatchMake<Fs>(ar)... };
	}
#if defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif

	/**
	 * @brief One clause per field, and the fields come as a pack straight off the ladder rather
	 * than index by index - MemberTypeT would ask tuple_element for each of them, for a
	 * list the ladder produced whole.
	 */
	template<class T, class... Fs>
	constexpr T makeOneClausePerField(Reader auto& ar, ::base::TypeList<Fs...>) {
		// No flattening here, so an array field has nowhere to go: it would need a
		// clause of its own, and no expression yields an array to fill one with.
		// Saying so beats the raw "no matching overloaded function" from below.
		static_assert(
			!(::std::is_array_v<Fs> || ... || false),
			"ser: this type is not an aggregate and has a C array field, so it cannot be "
			"built one clause per field - braces here call a CONSTRUCTOR, and nothing "
			"returns an array to pass to it. Write serMake by hand and name the elements:\n"
			"  static T serMake(ser::Reader auto& ar) {\n"
			"      return T{ ser::subMake<E>(ar), ser::subMake<E>(ar), ... };\n"
			"  }\n"
			"Braced, so the reads stay in order. Or hold the field as std::array, which "
			"is a class and can be built and returned like any other field."
		);

		return T{ dispatchMake<Fs>(ar)... };
	}

	/**
	 * @brief Flattening is for AGGREGATES only. Braces on anything else call a constructor, and
	 * a constructor takes one argument per field - handing it three ints for an array
	 * parameter would be a different overload, not brace elision.
	 */
	template<class T>
	constexpr T makeByFields(Reader auto& ar) {
		checkWalkableFields<T>();
		if constexpr (::std::is_aggregate_v<T>)
			return makeFromClauses<T>(ar, FlatFieldsT<T>{});
		else
			return makeOneClausePerField<T>(ar, FieldTypesT<T>{});
	}

	/**
	 * @brief the same fields, through parentheses
	 * @details For a type whose braces are a list - a std::initializer_list constructor - braces
	 * reach the wrong constructor, and parentheses are the only way to the right one.
	 * Parentheses do not order their arguments, so the fields are read into a tuple
	 * first: that IS a braced list, and [dcl.init.list]/4 orders its clauses left to
	 * right. Then apply calls the constructor with what was already read.
	 *
	 * The price is one move per field, which is why braces stay the default.
	 */
	template<class T, class... Fs>
	constexpr T makeParenFromList(Reader auto& ar, ::base::TypeList<Fs...>) {
		static_assert(
			!(::std::is_array_v<Fs> || ... || false),
			"ser: this type has a C array field, and a constructor cannot be handed an "
			"array by value. Read into an existing object instead - T obj; ser::In{bytes}(obj); "
			"- or hold the field as std::array, which is a class and can be passed."
		);

		::std::tuple<Fs...> fields{ dispatchMake<Fs>(ar)... };
		return ::std::apply([](auto&... f) { return T(::std::move(f)...); }, fields);
	}

	template<class T>
	constexpr T makeParenByFields(Reader auto& ar) {
		return makeParenFromList<T>(ar, FieldTypesT<T>{});
	}

	/**
	 * @brief The one array field that must NOT be flattened: one with a serializer of its own,
	 * other than the element-by-element one every array has. The write side would use that
	 * hook, and reading the elements back one by one would then be a different format. Such a
	 * type keeps the refusal below.
	 */
	template<class F, class Ar>
	inline constexpr bool HAS_OWN_ARRAY_SERIALIZER_V
		= ::std::is_array_v<F> && HAS_CUSTOM_SERIALIZER_V<F, Ar>
	   && !::std::is_base_of_v<ElementwiseArray, Serializer<::std::remove_cv_t<F>>>;

	template<class T, class Ar, ::std::size_t... I>
	consteval bool arrayFieldHasOwnSerializer(::std::index_sequence<I...>) {
		return (HAS_OWN_ARRAY_SERIALIZER_V<MemberTypeT<T, I>, Ar> || ... || false);
	}

	template<class T, class Ar>
	inline constexpr bool AGGREGATE_MAKEABLE_V
		= ::std::is_aggregate_v<T> && CAN_ENUMERATE_MEMBERS_V<T>
	   && !arrayFieldHasOwnSerializer<T, Ar>(::std::make_index_sequence<MEMBER_COUNT_V<T>>{});

	/**
	 * @brief Returns by value so that types without a default constructor, with const fields,
	 * or non-movable ones can be built directly into their final storage. Errors travel
	 * as an exception because there is no room for a return code next to the value -
	 * ser::read catches it at the boundary. This is the only place with exceptions.
	 */
	template<class T, Reader Ar>
	requires(!::std::is_array_v<T>) constexpr T dispatchMake(Ar& ar) {
		checkHooks<T, Ar>();

		// A serMake that reads its fields with subMake recurses through here without
		// ever touching dispatchRead, so this path needs its own guard.
		DepthGuard g{ ar };
		if (!g.entered()) throwError(Errc::DepthExceeded, ar.position());

		if constexpr (builtin::DENIED_V<T>) {
			builtin::deny<T>();
			throwError(Errc::InvalidValue, ar.position());
		}

		else if constexpr (HAS_MAKE_V<Access::TraitHooks, T, Ar>)
			return Serializer<T>::make(ar);                     // 1
		else if constexpr (HAS_MAKE_V<Access::MemberHooks, T, Ar>)
			return Access::callMake<T>(ar);                     // 2
		else if constexpr (HAS_MAKE_V<AdlHooks, T, Ar>)
			return adl_barrier::callMake(ar, ::ser::Tag<T>{});  // 3

		// Every one of those three returns the hook's prvalue straight out of a return
		// statement, so the object is built once, in the caller's storage. That is what
		// makes a non-movable type readable at all.

		// `return x;` is NRVO - permitted, not guaranteed - so a move constructor must exist
		// as the fallback. All three conditions are asked in the CONDITION so that a type
		// failing any of them falls through to step 4b, which builds T from prvalues and
		// needs none of them. Declining here also breaks the cycle with step 9 of
		// dispatchRead, which comes back into this function.
		else if constexpr (::std::default_initializable<T> && ::std::move_constructible<T>
		                   && !MUST_BE_BUILT_V<T, Ar>) {  // 4
			T x{};
			if (const auto e = dispatchRead<T>(ar, x); e != Errc::Ok) throwError(e, ar.position());
			return x;
		}
		// Step 4b. An aggregate that step 4 declined: no default constructor, a const
		// field, or nothing to move it with. Aggregate initialization needs none of the
		// three - it builds every field in place from a prvalue.
		else if constexpr (AGGREGATE_MAKEABLE_V<T, Ar>)  // 4b
			return makeByFields<T>(ar);

		else if constexpr (MUST_BE_BUILT_V<T, Ar> && !::std::is_aggregate_v<T>)
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: this type cannot be filled in place - a const field or a BIT-FIELD, and "
				"neither can be written through a reference - and it is not an aggregate, so it "
				"cannot be built from its fields either. Braces are what initialize a field by "
				"value, and only an aggregate has them. Give it SER_DESCRIBE_MAKE(Type, a, b): "
				"its read side builds through the constructor and reads each field BY VALUE, "
				"which a bit-field and a const field both accept. SER_MAKE_FROM alone is not "
				"enough - it supplies only the read side, and the pairing rule wants both."
			);

		else if constexpr (::std::default_initializable<T>)
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,  // 4c
				"ser: this type is default constructible but not movable, so it cannot be "
				"returned by value. Read it in place instead: T obj; ser::In{bytes}(obj). "
				"Or give it a serMake hook - a hook's result is returned directly and is "
				"never moved."
			);

		else if constexpr (::std::is_aggregate_v<T> && CAN_ENUMERATE_MEMBERS_V<T>)  // 4d
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: this type has an array field with a serializer of its own, and no way "
				"to be constructed. An array field is normally built one element per "
				"initializer clause, but that would ignore the array's own serializer and "
				"read a different format than was written. Give the type a default "
				"constructor and read into it - T obj; ser::In{bytes}(obj); - or give the "
				"type itself a serMake hook."
			);

		// The same refusal the other two contexts have, and it belongs here most of all: an
		// aggregate with a const ARRAY field arrives here and nowhere else, and without this
		// rung it would get the generic message below, whose suggestions are all about
		// something other than what is wrong.
		else if constexpr (::std::is_aggregate_v<T> && !CAN_ENUMERATE_MEMBERS_V<T>)  // 4e
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: this type is an aggregate whose field count cannot be worked out, "
				"so it cannot be built field by field. A C ARRAY field elides braces, so "
				"counting sees its elements rather than the field; a BASE CLASS is an "
				"element of aggregate initialization but not of a structured binding. "
				"Declare the count in the class:\n"
				"  using ser_members = ser::Members<N>;\n"
				"A base that has data members of its own cannot be decomposed at all - "
				"give the type a serMake hook instead."
			);

		else if constexpr (NONGENERIC_ANY_HOOK_V<T, Ar>)
			throwError(Errc::InvalidValue, ar.position());
		else if constexpr (builtin::looksPointerLike<T>()) {
			builtin::denyPointerLike<T>();
			throwError(Errc::InvalidValue, ar.position());
		} else
			static_assert(
				::base::DEPENDENT_FALSE_V<T>,
				"ser: don't know how to construct this type.\n"
				"  Has a constructor?          SER_MAKE_FROM(Type, a, b, c);\n"
				"  Not your type?              ser::Serializer<T>::make\n"
				"  Has non-public fields?      SER_MAKE_FROM, or friend ser::Access + Members<N>"
			);
	}

}  // namespace ser::internal
