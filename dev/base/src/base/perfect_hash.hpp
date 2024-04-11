#pragma once

#include "ints.hpp"
#include <type_traits>

namespace base {

	using HashT = u64;

	HashT perfectHash(u64 v) { return v; }

	namespace detail {
		template<typename T>
      	concept MemberHash = requires(T& t) {
			{ t.perfectHash() } -> std::same_as<HashT>;
		};


		template<class T>
		auto perfectHashCPO(const T& key) {
			if constexpr (MemberHash<T>) {
				return key.perfectHash();
			}
			else {
				return perfectHash(key);
			}
		}
	}

	template<class T>
	auto perfectHash(const T& key) {
		return detail::perfectHashCPO(key);
	}
}

