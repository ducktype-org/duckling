#pragma once

#include <result.hpp>

using cpp::fail;
using cpp::failure;
using cpp::result;

namespace internal {
	struct NoneVariant {};
}

template<class T>
using option = result<T, internal::NoneVariant>;

template<class T>
option<T> none() {
	return failure(internal::NoneVariant{});
}

template<class T>
using some = option<T>::result;

template<class T, class E, class R>
R convertResult(result<T, E> result, std::function<R(const T&)> map_value,
                std::function<R(const E&)> map_error) {
	if (result.has_value()) return map_value(result.value());
	return map_error(result.error());
}

template<class E, class R>
R convertResult(result<void, E> result, std::function<R()> map_value,
                std::function<R(const E&)> map_error) {
	if (result.has_value()) return map_value();
	return map_error(result.error());
}

template<class T, class E>
result<T, E> ok(T x) {
	return result<T, E>(x);
}

template<class E>
result<void, E> ok() {
	return result<void, E>();
}

template<class T, class E>
void ignore([[maybe_unused]] result<T, E>) {}
