#pragma once

#include <base/comptime/member_walk.hpp>
#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/preproc/ladder.hpp>
#include <base/preproc/stringify.hpp>

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/internal/adl.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/hooks.hpp>
#include <ser/serializer.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser {

	/**
	 * @brief ser::Members<N>
	 * @details The opt-in field count. It skips the counting phase and, together with
	 * `friend ser::Access`, is what lets the structured-bindings ladder see private
	 * fields. A wrong N is a compile error, so the format cannot drift silently.
	 */
	template<::std::size_t N>
	struct Members final {
		static constexpr ::std::size_t COUNT = N;
	};

	/**
	 * @brief ser::Access
	 * @details Every in-class hook detector lives HERE rather than in ser::internal because access
	 * control is part of SFINAE: a detector written in ser::internal reports false for a
	 * private serVisit that ser::Access can call perfectly well. The trait-level detectors
	 * have no access problem and live here anyway, so the whole priority table reads in one
	 * place.
	 *
	 * T carries the cv-qualification of the object for the symmetric `visit` form, so a hook
	 * binding only a non-const object is invisible on the write side - which
	 * detail::checkHooks reports instead of letting the two directions use different formats.
	 */
	struct Access final {
		/**
		 * @brief level 1: ser::Serializer<T>
		 * @details The probes the questions in detail/hooks.hpp are asked with: declared, never
		 * defined, one per hook form. The trait level needs no access rights of its own,
		 * but it lives here so the whole priority table reads in one place.
		 */
		struct TraitHooks final {
			template<class Ar, class T>
			static auto visit(Ar& ar, T& x)
				-> decltype(Serializer<::std::remove_cvref_t<T>>::visit(ar, x));

			template<class Ar, class T>
			static auto write(Ar& ar, const T& x)
				-> decltype(Serializer<::std::remove_cvref_t<T>>::write(ar, x));

			/**
			 * @brief Forwarding, so this one probe answers both "does it fill an lvalue" and
			 * "does it also swallow an rvalue" - see HAS_READ_RVALUE_V.
			 */
			template<class Ar, class T>
			static auto read(Ar& ar, T&& x)
				-> decltype(Serializer<::std::remove_cvref_t<T>>::read(ar, ::std::forward<T>(x)));

			/** @brief T first: there is no argument to deduce it from. */
			template<class T, class Ar>
			static auto make(Ar& ar) -> decltype(Serializer<::std::remove_cvref_t<T>>::make(ar));
		};

		/**
		 * @brief level 2: hooks declared in the class
		 * @details These are the probes that make access control part of SFINAE: written in
		 * ser::internal they could not name a private serVisit, and every detector built on
		 * them would report false for a hook that works perfectly well. A nested class of a
		 * befriended class has the same access rights the friend does.
		 */
		struct MemberHooks final {
			template<class Ar, class T>
			static auto visit(Ar& ar, T& x) -> decltype(::std::remove_cvref_t<T>::serVisit(ar, x));

			template<class Ar, class T>
			static auto write(Ar& ar, const T& x)
				-> decltype(::std::remove_cvref_t<T>::serWrite(ar, x));

			template<class Ar, class T>
			static auto read(Ar& ar, T&& x)
				-> decltype(::std::remove_cvref_t<T>::serRead(ar, ::std::forward<T>(x)));

			template<class T, class Ar>
			static auto make(Ar& ar) -> decltype(::std::remove_cvref_t<T>::serMake(ar));
		};

		/**
		 * @brief "the name is there, but this archive cannot call it"
		 * @details An id-expression for a function TEMPLATE cannot be formed without arguments to
		 * deduce from, so each of these is true for exactly a single NON-template
		 * declaration - a hook pinned to one concrete archive type. Compared against the
		 * archive in use, that is how a hook that works is told from a hook that is being
		 * silently ignored.
		 */
		template<class T>
		static constexpr bool NAMES_TRAIT_VISIT_V
			= requires { Serializer<::std::remove_cvref_t<T>>::visit; };
		template<class T>
		static constexpr bool NAMES_TRAIT_WRITE_V
			= requires { Serializer<::std::remove_cvref_t<T>>::write; };
		template<class T>
		static constexpr bool NAMES_TRAIT_READ_V
			= requires { Serializer<::std::remove_cvref_t<T>>::read; };
		template<class T>
		static constexpr bool NAMES_TRAIT_MAKE_V
			= requires { Serializer<::std::remove_cvref_t<T>>::make; };

		template<class T>
		static constexpr bool NAMES_MEMBER_VISIT_V
			= requires { ::std::remove_cvref_t<T>::serVisit; };
		template<class T>
		static constexpr bool NAMES_MEMBER_WRITE_V
			= requires { ::std::remove_cvref_t<T>::serWrite; };
		template<class T>
		static constexpr bool NAMES_MEMBER_READ_V = requires { ::std::remove_cvref_t<T>::serRead; };
		template<class T>
		static constexpr bool NAMES_MEMBER_MAKE_V = requires { ::std::remove_cvref_t<T>::serMake; };

		/** @brief "the hook is there, but it is not static" */
		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_VISIT_V = requires(Ar& ar, ::std::remove_cvref_t<T>& t) {
			t.serVisit(ar);
		} || requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serVisit(ar, t); };

		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_WRITE_V
			= requires(Ar& ar, const ::std::remove_cvref_t<T>& c) { c.serWrite(ar); }
		   || requires(Ar& ar, const ::std::remove_cvref_t<T>& c) { c.serWrite(ar, c); }
		   || requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serWrite(ar); };

		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_READ_V = requires(Ar& ar, ::std::remove_cvref_t<T>& t) {
			t.serRead(ar);
		} || requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serRead(ar, t); };

		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_MAKE_V
			= requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serMake(ar); };

		/**
		 * @brief the calls
		 * @details These are the reason a private hook works at all: the call site is inside the
		 * class the type befriended. dispatch never names T::ser* directly.
		 */
		template<class T, class Ar>
		static constexpr Errc callVisit(Ar& ar, T& x) {
			return ::std::remove_cvref_t<T>::serVisit(ar, x);
		}

		template<class T, class Ar>
		static constexpr Errc callWrite(Ar& ar, const T& x) {
			return T::serWrite(ar, x);
		}

		template<class T, class Ar>
		static constexpr Errc callRead(Ar& ar, T& x) {
			return T::serRead(ar, x);
		}

		/** @brief T first: there is no argument to deduce it from. */
		template<class T, class Ar>
		static constexpr T callMake(Ar& ar) {
			return T::serMake(ar);
		}

		/**
		 * @brief the opt-in field count
		 * @details `using ser_members = ser::Members<2>;` inside the class, private if you like -
		 * which is why the detector is here. It exists because COUNTING and DECOMPOSING
		 * have different requirements: decomposition works on any class whose members are
		 * accessible here, while the count is probed with aggregate initialization, which a
		 * non-aggregate does not support.
		 */
		template<class T>
		static constexpr bool NAMES_MEMBER_COUNT_V
			= requires { ::std::remove_cvref_t<T>::ser_members::COUNT; };

		template<class T>
		static constexpr ::std::size_t declaredMemberCount() {
			return ::std::remove_cvref_t<T>::ser_members::COUNT;
		}

		/**
		 * @brief what SER_DESCRIBE left behind
		 * @details SER_DESCRIBE is usually written in a private section, so a detector in
		 * ser::internal reports false for it and the caller then compares EVERY member - including
		 * the ones the description deliberately left out, which come back default-constructed.
		 */
		template<class T>
		static constexpr bool HAS_DESCRIBED_V = requires(const ::std::remove_cvref_t<T>& x) {
			::std::remove_cvref_t<T>::ser_described(x);
		};

		template<class T>
		static constexpr auto described(const T& x) {
			return ::std::remove_cvref_t<T>::ser_described(x);
		}

		template<class T>
		static constexpr bool HAS_FIELD_NAMES_V
			= requires { ::std::remove_cvref_t<T>::ser_field_names; };

		template<class T>
		static constexpr const char* fieldName(::std::size_t i) {
			return ::std::remove_cvref_t<T>::ser_field_names[i];
		}

		template<class T>
		static constexpr bool HAS_SERIALIZE_AS_V
			= requires { typename ::std::remove_cvref_t<T>::ser_serialize_as; };

		template<class T>
		using SerializeAsT = typename ::std::remove_cvref_t<T>::ser_serialize_as;

		template<class T>
		static constexpr bool HAS_SCHEMA_TAG_V = requires {
			{ ::std::remove_cvref_t<T>::ser_schema_tag } -> ::std::convertible_to<const char*>;
		};

		template<class T>
		static constexpr const char* schemaTag() {
			return ::std::remove_cvref_t<T>::ser_schema_tag;
		}

		/**
		 * @brief the structured-bindings ladder
		 * @details Calls f with every member of obj as an lvalue. The arity is a template
		 * parameter rather than something computed here, so this stays free of
		 * describe_bind.hpp and the include graph has no cycle: counting needs the
		 * ladder (MemberTypeT), the ladder needs nothing.
		 * @note The ladder binds obj and calls f in place; forwarding either would change what
		 * the members bind as, which is the whole contract of the walk.
		 */
		/** @brief The member count has to have a rung in the structured-bindings ladder. */
		template<::std::size_t N>
		static consteval void checkArity() {
			static_assert(
				N <= ::base::LADDER_MAX,
				"ser: this type has more than " STRINGIFY_2(BASE_LADDER_MAX
			    ) " members "
				  "(::base::LADDER_MAX), the limit of the structured-bindings ladder. Split it, "
				  "or give it a serVisit hook."
			);
		}

		template<::std::size_t N, class T, class F>
		// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
		static constexpr decltype(auto) visitMembersN(T&& obj, F&& f) {
			checkArity<N>();
			if constexpr (N == 0) {
				static_assert(
					::std::is_empty_v<::std::remove_cvref_t<T>>,
					"ser: this type declares zero members - `using ser_members = "
					"ser::Members<0>;` - but it has data. That format writes no bytes and a "
					"read leaves every field default-constructed, silently. Declare the real "
					"count, or drop the declaration and let the walk count the fields."
				);
				(void) obj;
				return f();
			}
			BASE_LADDER(BASE_LADDER_WALK)
		}

		/**
		 * @brief The declared type of every field, which only a structured binding can still
		 * see - see base::FieldDecl in base/comptime/type_list.hpp. Never called either:
		 * callers ask for decltype of it.
		 */
		template<::std::size_t N, class T>
		// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
		static constexpr auto fieldDeclsN(T&& obj) {
			checkArity<N>();
			if constexpr (N == 0) {
				(void) obj;
				return ::base::TypeList<>{};
			}
			BASE_LADDER(BASE_LADDER_DECLS)
		}
	};

	/**
	 * @brief Reads one sub-object off the stream, for a hand-written `serMake`.
	 * @details `serMake` builds its object rather than filling one, so it needs the fields as
	 * values before the constructor runs. Braces are what guarantee several of these happen in
	 * order: `return T{ ser::subMake<A>(ar), ser::subMake<B>(ar) };`
	 * @note Signals by THROWING a `ser::Exception`, like everything on the make path - a
	 * constructor argument has nowhere to put an error code. `ser::read` turns it back into a
	 * code at the boundary.
	 */
	template<class F, Reader Ar>
	[[nodiscard]] constexpr F subMake(Ar& ar) {
		return internal::dispatchMake<F>(ar);
	}

/**
 * @brief A type opts in with one line. `friend ser::Access;` works just as well; the macro
 * exists so the spelling does not have to be remembered.
 */
#define SER_FRIEND friend struct ::ser::Access;

}  // namespace ser
