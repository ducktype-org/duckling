#pragma once

#include <base/bit256.hpp>
#include <base/ints.hpp>

namespace query {
	using QueryUnstableHash = u64;
	using QueryStableHash   = base::Bit256;

	// Concept to validate queryUnstablePerfectHash signature
	template<typename T>
	concept HasUnstablePerfectHash = requires(T t) {
		{ t.queryUnstablePerfectHash() } -> std::same_as<QueryUnstableHash>;
	};

	// Concept to validate queryStablePerfectHash signature
	template<typename T>
	concept HasStablePerfectHash = requires(T t) {
		{ t.queryStablePerfectHash() } -> std::same_as<QueryStableHash>;
	};

	template<class T>
	struct queryUnstableHashFunctor final {
		std::size_t operator()(const T& key) const {
			if constexpr (std::is_same_v<std::remove_cvref_t<T>, u64>)
				return key;
			else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>)
				return static_cast<u64>(key);
			else
				return key.queryUnstablePerfectHash();
		}
	};
}
