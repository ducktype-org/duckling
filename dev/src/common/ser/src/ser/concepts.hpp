#pragma once

#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	template<class A>
	concept archive = requires(A& a) {
		{ a.position() } -> ::std::convertible_to<::std::size_t>;
	};

	template<class A>
	concept writer = archive<A> && A::IS_WRITING;

	template<class A>
	concept reader = archive<A> && A::IS_READING;

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

	// A cheap local guard, never a bulk-copy gate: it lets through padding and any type
	// that has a serializer of its own.
	template<class T>
	concept trivially_serializable = ::std::is_trivially_copyable_v<T> && !::std::is_pointer_v<T>;

}  // namespace ser
