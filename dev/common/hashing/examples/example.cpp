#include <iostream>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <base/ints.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hash.hpp>
#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <hashing/type_hash_code.hpp>
#include <hashing/type_unique_code.hpp>

// example usage of hashing utilities

// we can use the Hash as a drop-in replacement for std::hash, for example in std::unordered_map
namespace my_map {
	template<
		class Key,
		class T,
		class Hash  = hashing::Hash<>,
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

	// by default fnva_64 algorithm is used
	std::cout << Hash{}(type1{}) << '\n';  // some 64-bit number
	// but we can specify the algorithm explicitly as a template parameter
	// it's also possible to get the hash value at compile time
	constexpr auto h = Hash<Fnv1a_32, TypeCode<u32, false>>{}(type2{});
	std::cout << h << '\n';  // some 32-bit number

	// by default the type's hash-code is appended to the hashed bytes so it's possible to
	// differentiate between hashes of pair<int, int>{1, 2} and array<int, 2>{1, 2}, but
	// if we want to, this can be turned off
	struct type3 {
		int x{ 123 }, y{ 456 };
	};

	struct type4 {
		std::array<int, 2> a{ 123, 456 };
	};

	std::cout << "different hashes:\n\t" << Hash{}(type3{}) << "\n\t" << Hash{}(type4{}) << '\n';
	std::cout << "the same hashes:\n\t" << Hash<Fnv1a_64, void>{}(type3{}) << "\n\t"
			  << Hash<Fnv1a_64, void>{}(type4{}) << '\n';

	// we can also visualize the bytes that were hashed
	std::cout << "notice 4 bytes starting from yellow ones, this is the type's hash-code:\n"
			  << Hash<DebugHash>{}(type3{}) << '\n'
			  << Hash<DebugHash>{}(type4{}) << '\n';

	std::cout << "here the type's hash-code is not appended:\n"
			  << hashing::Hash<hashing::DebugHash, void>{}(type3{}) << '\n'
			  << hashing::Hash<hashing::DebugHash, void>{}(type4{}) << '\n';

	// there is also a stateful hash that can be used to Hash multiple objects together
	hashing::StatefulHash<hashing::DebugHash> hasher2;
	// we can add objects one by one
	hasher2(7);
	hasher2(type2{});
	// or all at once
	hasher2(7, std::string{ "hello" }, 42);
	constexpr auto hash_value = hashing::StatefulHash<hashing::Fnv1a_64, TypeCode<u32, false>>{}(
									7, type2{}, 7, std::string{ "hello" }, 42
	)
	                                .finalize();
	std::cout << "stateful hash:\n"
			  << hasher2.finalize() << "\n\t(constexpr) hash value: " << hash_value << '\n';

	// module also provides unique ids for types in compile time
	// note that those can change between compilations
	std::cout << "constexpr hash codes:\t" << hashing::TYPE_HASH_CODE<int> << ' '
			  << hashing::TYPE_HASH_CODE<type1> << '\n';

	std::cout << "hash of type_with_bases: " << hashing::Hash{}(type_with_bases{}) << '\n';


	// different hash code types
	std::cout << "using different hash code types (none, 4 bytes, 8 bytes):\n"
			  << hashing::Hash<hashing::DebugHash, void>{}(42) << '\n'
			  << hashing::Hash<hashing::DebugHash, hashing::TypeCode<u32>>{}(42) << '\n'
			  << hashing::Hash<hashing::DebugHash, hashing::TypeCode<u64>>{}(42) << '\n';

	// debug hash with tuple
	std::cout << "hashing tuple-like types:\n"
			  << hashing::StatefulHash<hashing::DebugHash>{}(
					 std::tuple{ 42, 3.14, "hello" }, std::pair<std::string, char>{ "abc", 'x' }
				 )
					 .finalize()
			  << '\n';

	// unique codes
	std::cout << "unique codes:\n"
			  << "\tint:\t" << hashing::TYPE_UNIQUE_CODE<int> << '\n'
			  << "\tt1:\t" << hashing::TYPE_UNIQUE_CODE<type1> << '\n'
			  << "\tt2:\t" << hashing::TYPE_UNIQUE_CODE<type2> << '\n'
			  << "\tint:\t" << hashing::TYPE_UNIQUE_CODE<int> << '\n';
}
