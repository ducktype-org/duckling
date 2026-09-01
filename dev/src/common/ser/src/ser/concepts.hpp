#pragma once

#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	/**
	 * @brief Anything that behaves as a stream: it knows where it is.
	 * @details The structural half only. It says nothing about direction - `reader` and
	 * `writer` add that, and `reader_or_writer` is the union a symmetric hook wants.
	 */
	template<class A>
	concept archive = requires(A& a) {
		{ a.position() } -> ::std::convertible_to<::std::size_t>;
	};

	/** @brief An archive in the writing direction: what `serWrite` takes. */
	template<class A>
	concept writer = archive<A> && A::IS_WRITING;

	/** @brief An archive in the reading direction: what `serRead` and `serMake` take. */
	template<class A>
	concept reader = archive<A> && A::IS_READING;

	/**
	 * @brief Either direction: what a symmetric `serVisit` hook takes.
	 * @details `serVisit` serves both directions from one body, so its archive parameter is
	 * neither a `reader` nor a `writer` in particular. Spelling this instead of a bare `auto&`
	 * is optional, and says the intention out loud the way `serWrite` and `serRead` already do
	 * with `writer` and `reader`:
	 *
	 *     static ser::Errc serVisit(ser::reader_or_writer auto& ar, auto& self) {
	 *         return ar(self.a, self.b);
	 *     }
	 *
	 * @note `self` stays `auto&`, and that is not laziness: the write direction hands the hook
	 * a CONST object, so pinning the type there would refuse it and break writing.
	 */
	template<class A>
	concept reader_or_writer = reader<A> || writer<A>;

	template<class B>
	concept byte_buffer = requires(B& b, const B& cb) {
		{ cb.size() } -> ::std::convertible_to<::std::size_t>;
		{ b.data() } -> ::std::convertible_to<::std::byte*>;
	};

	template<class B>
	concept resizable_buffer = byte_buffer<B> && requires(B& b, ::std::size_t n) { b.resize(n); };

	template<class B>
	concept fixed_buffer = byte_buffer<B> && !resizable_buffer<B>;

	template<class T>
	concept byte_like = ::std::same_as<::std::remove_cv_t<T>, ::std::byte>;

	/**
	 * @brief A cheap local guard, never a bulk-copy gate: it lets through padding and any type
	 * that has a serializer of its own.
	 */
	template<class T>
	concept trivially_serializable = ::std::is_trivially_copyable_v<T> && !::std::is_pointer_v<T>;

} /* namespace ser */
