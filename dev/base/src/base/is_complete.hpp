#pragma once

#include <type_traits>

template<typename, typename = void>
struct IS_COMPLETE {
	static constexpr bool VALUE = false;
};

template<typename T>
struct IS_COMPLETE<T, std::void_t<decltype(sizeof(T))>> {
	static constexpr bool VALUE = true;
};

template<typename T, typename S = void>
constexpr bool IS_COMPLETE_V = IS_COMPLETE<T, S>::VALUE;
