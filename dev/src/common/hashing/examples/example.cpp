// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/types/ints.hpp>
#include <base/types/monostate.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/by_address.hpp>
#include <hashing/hash.hpp>
#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>

#include <iostream>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

// example usage of hashing utilities

// we can use the Hash as a drop-in replacement for std::hash, for example in std::unordered_map
namespace my_map {
	using Hasher = decltype([](auto&& x) { return hashing::Hash<>{}(x).data.at(0); });

	template<
		class Key,
		class T,
		class Hash  = Hasher,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>
	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

// to hook our own type we can simply tell the machinery which subobjects of our type to hash
struct type1 {
	int         size{ 7 };
	int         capacity{ 42 };
	std::string s{ "hello" };

	friend constexpr auto hashDecompose(const type1& t) noexcept { return std::tie(t.size, t.s); }
};

// or for more fine-grained control we can define under which conditions, which subobjects/bytes
// should be hashed
struct type2 {
	int         x{ 42 };
	bool        b{ true };
	std::string s{ "hello" };

	friend constexpr void addToHash(hashing::hash_algorithm auto& h, const type2& t) noexcept {
		addToHash(h, t.x);
		if (t.b) addToHash(h, t.s);
	}
};

struct type_with_bases: type1, type2 {
	int x{ 123 }, y{ 456 };

	friend constexpr auto hashDecompose(const type_with_bases& t) {
		return std::tie(hashing::getBase<type1>(t), hashing::getBase<type2>(t), t.x, t.y);
	}
};

// types with a unique representation in memory can simply opt in to being hashed as bytes
// (note: these have to live at namespace scope - a local class cannot have static data members)
struct type3 {
	int x{ 123 }, y{ 456 };

	static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};
};

struct type4 {
	std::array<int, 2> a{ 123, 456 };

	static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};
};

// example implementation of a (very poor) hash algorithm
struct ExampleHash {
	u64 state{ 0 };
	using result_type = u64;

	// 1) setup - prepare the state
	constexpr ExampleHash(u64 state = 0) noexcept: state{ state } {}

	// 2) update - consume the data updating the state
	constexpr void operator()(const std::span<const std::byte> span) noexcept {
		for (auto&& c: span) state ^= std::to_integer<unsigned char>(c);
	}

	// 3) finalize - convert the state to the result
	[[nodiscard]]
	constexpr u64 finalize() const noexcept {
		return state;
	}
};

static_assert(hashing::hash_algorithm<ExampleHash>);

int main() {
	using namespace hashing;

	my_map::unordered_map<int, int> m;
	m[1] = 2;
	m[2] = 3;
	std::cout << m[1] << ' ' << m[2] << '\n';  // 2 3

	// by default the DefaultHashAlgorithm is used
	std::cout << Hash{}(type1{}) << '\n';

	// but we can specify the algorithm explicitly as a template parameter
	// it's also possible to get the hash value at compile time
	constexpr auto H = Hash<SHA256>{}(type2{});
	std::cout << H << '\n';  // some 256-bit number

	// we can also visualize the bytes that were hashed
	std::cout << Hash<DebugHash>{}(type3{}) << '\n' << Hash<DebugHash>{}(type4{}) << '\n';
	// there is also a stateful hash that can be used to Hash multiple objects together
	hashing::StatefulHash<hashing::DebugHash> hasher2;

	// we can add objects one by one
	hasher2(7);
	hasher2(type2{});

	// or all at once
	hasher2(7, std::string{ "hello" }, 42);

	constexpr auto HASH_VALUE
		= hashing::StatefulHash<hashing::SHA256>{}(7, type2{}, 7, std::string{ "hello" }, 42)
	          .finalize();
	std::cout << "stateful hash:\n"
			  << hasher2.finalize() << "\n\t(constexpr) hash value: " << HASH_VALUE << '\n';


	std::cout << "hash of type_with_bases: " << hashing::Hash{}(type_with_bases{}) << '\n';


	// debug hash with tuple
	// note the explicit string_view: `std::tuple{ 42, 3.14, "hello" }` would deduce a
	// `const char*` member, and raw pointers are not hashable - see below
	std::cout << "hashing tuple-like types:\n"
			  << hashing::StatefulHash<hashing::DebugHash>{}(
					 std::tuple{ 42, 3.14, std::string_view{ "hello" } },
					 std::pair<std::string, char>{ "abc", 'x' }
				 )
					 .finalize()
			  << '\n';

	// pointers are never hashed implicitly - either hash the pointee, or say explicitly that
	// the address (i.e. the identity of the pointee) is what should be hashed
	const type1        t1;
	const type1* const ptr = &t1;
	std::cout << "hash of the pointee: " << hashing::Hash{}(*ptr) << '\n'
			  << "hash of the address: " << hashing::Hash{}(hashing::HashByAddress{ ptr }) << '\n';
}
