#pragma once

/*
 * std::pair and std::tuple
 * The elements in order, nothing else: no count, because the type carries it, and no
 * padding. A pair is its first element followed by its second, which is what makes
 * pair<K, V> and a two-field aggregate the same bytes.
 *
 * Both a read and a make. `read` fills an existing tuple and is constrained on the elements
 * being assignable, so a tuple with a const element simply does not have it and dispatch
 * moves on to `make`. `make` builds one IN BRACES, because [dcl.init.list]/4 orders the
 * clauses left to right while constructor arguments are not ordered at all - anything else
 * would make the byte order compiler-dependent.
 */

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/fillable.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser {

	template<class A, class B>
	struct MinWireSize<::std::pair<A, B>> {
		static constexpr ::std::size_t VALUE = MIN_WIRE_SIZE_V<A> + MIN_WIRE_SIZE_V<B>;
	};

	template<class... Es>
	struct MinWireSize<::std::tuple<Es...>> {
		static constexpr ::std::size_t VALUE = (::std::size_t{ 0 } + ... + MIN_WIRE_SIZE_V<Es>);
	};

	namespace internal {

		/**
		 * @brief Shared by both, so pair and tuple cannot drift apart in a way the format would
		 * notice. Es are the element types with cv stripped - a const element is written
		 * and read as its underlying type.
		 */
		template<class T, class... Es>
		constexpr Errc writeElements(Writer auto& ar, const T& t) {
			Errc code = Errc::Ok;
			[&]<::std::size_t... I>(::std::index_sequence<I...>) {
				(void) ((code = dispatchWrite<Es>(ar, ::std::get<I>(t)), code == Errc::Ok) && ...);
			}(::std::index_sequence_for<Es...>{});
			return code;
		}

		template<class T, class... Es>
		constexpr Errc readElements(Reader auto& ar, T& t) {
			Errc code = Errc::Ok;
			[&]<::std::size_t... I>(::std::index_sequence<I...>) {
				(void) ((code = dispatchRead<Es>(ar, ::std::get<I>(t)), code == Errc::Ok) && ...);
			}(::std::index_sequence_for<Es...>{});
			return code;
		}

		template<class T>
		inline constexpr bool TUPLE_ELEMENTS_FILLABLE = false;

		template<class A, class B>
		inline constexpr bool TUPLE_ELEMENTS_FILLABLE<::std::pair<A, B>>
			= ::std::is_move_assignable_v<A> && ::std::is_move_assignable_v<B>;

		template<class... Es>
		inline constexpr bool TUPLE_ELEMENTS_FILLABLE<::std::tuple<Es...>>
			= (::std::is_move_assignable_v<Es> && ... && true);

	} /* namespace internal */

	/**
	 * @brief A pair is the same bytes as a two-field aggregate and hashes DIFFERENTLY, because
	 * "struct" and "pair" are different tokens. That is the conservative direction: a hash
	 * that says "changed" when the bytes did not costs a rebuilt cache, while the reverse
	 * costs a misread stream.
	 */
	template<class A, class B>
	struct Schema<::std::pair<A, B>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			h = internal::schemaText(h, "pair");
			h = internal::schemaOf<::std::remove_cv_t<A>, Mode, Seen>(h);
			return internal::schemaOf<::std::remove_cv_t<B>, Mode, Seen>(h);
		}
	};

	template<class... Es>
	struct Schema<::std::tuple<Es...>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			h = internal::schemaNumber(internal::schemaText(h, "tuple"), sizeof...(Es));
			((h = internal::schemaOf<::std::remove_cv_t<Es>, Mode, Seen>(h)), ...);
			return h;
		}
	};

	template<class A, class B>
	struct Serializer<::std::pair<A, B>> {
		using PairType = ::std::pair<A, B>;
		using First    = ::std::remove_cv_t<A>;
		using Second   = ::std::remove_cv_t<B>;

		static constexpr Errc write(Writer auto& ar, const PairType& p) {
			return internal::writeElements<PairType, First, Second>(ar, p);
		}

		static constexpr Errc read(Reader auto& ar, PairType& p)
			requires(internal::TUPLE_ELEMENTS_FILLABLE<PairType>) {
			return internal::readElements<PairType, First, Second>(ar, p);
		}

		static constexpr PairType make(Reader auto& ar)
			requires(internal::BUILDABLE_V<First> && internal::BUILDABLE_V<Second>) {
			return PairType{ internal::dispatchMake<First>(ar), internal::dispatchMake<Second>(ar) };
		}
	};

	template<class... Es>
	struct Serializer<::std::tuple<Es...>> {
		using TupleType = ::std::tuple<Es...>;

		static constexpr Errc write(Writer auto& ar, const TupleType& t) {
			return internal::writeElements<TupleType, ::std::remove_cv_t<Es>...>(ar, t);
		}

		static constexpr Errc read(Reader auto& ar, TupleType& t)
			requires(internal::TUPLE_ELEMENTS_FILLABLE<TupleType>) {
			return internal::readElements<TupleType, ::std::remove_cv_t<Es>...>(ar, t);
		}

		static constexpr TupleType make(Reader auto& ar)
			requires((internal::BUILDABLE_V<::std::remove_cv_t<Es>> && ... && true)) {
			return TupleType{ internal::dispatchMake<::std::remove_cv_t<Es>>(ar)... };
		}
	};

} /* namespace ser */
