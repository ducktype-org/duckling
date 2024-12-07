#include <iostream>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <iostream>

// #include <base/ints.hpp>
// #include <hashing/hash.hpp>
#include "../src/hashing/hash.hpp"

// example usage of hashing utilities

// we can use the hash as a drop-in replacement for std::hash, for example in std::unordered_map
namespace my_map {
	template<
		class Key,
		class T,
		class Hash  = hashing::hash<>,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>

	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

// we can hash our own types by telling the machinery which subobjects to hash
struct type1 {
	int         size{ 7 };
	int         capacity{ 42 };
	std::string s{ "hello" };

	friend constexpr auto hash_decompose(const type1& t) noexcept { return std::tie(t.size, t.s); }
};

// or for more fine-grained control we can define under which conditions, which subobjects/bytes
// should be hashed
struct type2 {
	int         x{ 7 };
	bool        b{ true };
	std::string s{ "hello" };

	friend constexpr void add_to_hash(hashing::hash_algorithm auto& h, const type2& t) noexcept {
		add_to_hash(h, t.x);
		if (t.b) add_to_hash(h, t.s);
	}
};

int main() {
	using namespace hashing;

	my_map::unordered_map<int, int> m;
	m[1] = 2;
	m[2] = 3;
	std::cout << m[1] << ' ' << m[2] << '\n';  // 2 3

	// by default fnva_64 algorithm is used
	std::cout << hash{}(type1{}) << '\n';  // some 64-bit number

	// but we can also specify the algorithm, it's also possible to get the hash value at compile
	// time
	// static_assert(!std::has_unique_object_representations_v<bool>);
	// constexpr auto h = hash<fnv1a_32>{}(type2{});
	// std::cout << h << '\n';  // some 32-bit number

	// by default the type's hash-code is appended to the hashed bytes so it's possible to
	// differentiate between hashes of pair(1, 2) and array<int, 2>{1, 2}, but if we want to this
	// can be turned off
	struct type3 {
		int x{ 1'234 }, y{ 9'876 };
	};

	struct type4 {
		std::array<int, 2> a{ 1'234, 9'876 };
	};

	std::cout << "different hashes:\n\t" << hash{}(type3{}) << "\n\t" << hash{}(type4{}) << '\n';
	std::cout << "the same hashes:\n\t" << hash<fnv1a_64, false>{}(type3{}) << "\n\t"
			  << hash<fnv1a_64, false>{}(type4{}) << '\n';

	// we can also visualize the bytes that were hashed
	std::cout << "notice 4 bytes starting from yellow ones, this is the type's hash-code:\n"
			  << hash<debug_hash>{}(type3{}) << '\n'
			  << hash<debug_hash>{}(type4{}) << '\n';

	std::cout << "here the type's hash-code is not appended:\n"
			  << hashing::hash<hashing::debug_hash, false>{}(type3{}) << '\n'
			  << hashing::hash<hashing::debug_hash, false>{}(type4{}) << '\n';

	// there is also a stateful hash that can be used to hash multiple objects together
	hashing::stateful_hash<hashing::debug_hash> hasher2;
	// we can add objects one by one
	hasher2(7);
	hasher2(type2{});
	// or all at once
	hasher2(7, std::string{ "hello" }, 42);
	constexpr auto hash_value = hashing::stateful_hash{}(7, type2{}, 7, std::string{ "hello" }, 42);
	std::cout << "stateful hash:\n"
			  << static_cast<std::string>(hasher2) << "\n\tconstexpr hash value: " << hash_value
			  << '\n';

	// module also provides unique ids for types in compile time
	// note that those can change between compilations
	std::cout << static_cast<u32>(hashing::type_hash_code<int>) << ' '
			  << static_cast<u32>(hashing::type_hash_code<type1>) << '\n';

	void f();
	f();
}

using namespace hashing;

struct X {
	int   x;
	int   y;
	float z;

public:
	friend constexpr auto hash_decompose(const X& q) noexcept { return std::tie(q.x, q.y, q.z); }
};

struct Y {
	float       f{};
	std::string s{};

public:
	friend constexpr auto hash_decompose(const Y& q) noexcept { return std::tie(q.f, q.s); }
};

struct Z {
	int   x;
	float y;
	Y     z;

public:
	friend constexpr auto hash_decompose(const Z& q) noexcept { return std::tie(q.x, q.y, q.z); }
};

struct S {
	int   x    = 5;
	int   y[2] = { 1, 2 };
	bool  b{};
	float z{ 7.0f };
	X     m{ 1, 2, 3.0f };
	Y     n{ 1.0f, "hello" };
	Z     o{ 1, 2.0f, { 1.0f, "hello" } };

public:
	friend constexpr void add_to_hash(hash_algorithm auto& h, const S& s) noexcept {
		add_to_hash(h, s.x);
		add_to_hash(h, s.y);
		if (s.b) add_to_hash(h, s.z);
		add_to_hash(h, s.m);
		add_to_hash(h, s.n);
		add_to_hash(h, s.o);
	}
};

// namespace my_map {
// 	template<
// 		class Key,
// 		class T,
// 		class Hash  = hash<>,
// 		class Pred  = std::equal_to<Key>,
// 		class Alloc = std::allocator<std::pair<const Key, T>>>

// 	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
// }

#include <cassert>

void f() {
	static_assert(type_hash_code<int> != type_hash_code<float>, "hash-codes should differ");
	static_assert(type_hash_code<double> != type_hash_code<char>, "hash-codes should differ");
	static_assert(type_hash_code<std::string> != type_hash_code<S>, "hash-codes should differ");
	static_assert(type_hash_code<X> == type_hash_code<X>, "hash-code should be the same");
	static_assert(
		std::convertible_to<decltype(type_hash_code<Y>), u32>,
		"hash-code should be convertible to u32"
	);

	static_assert(hash_algorithm<fnv1a_32>, "fnv1a_32 should be a hashing algorithm");
	static_assert(hash_algorithm<fnv1a_64>, "fnv1a_64 should be a hashing algorithm");
	static_assert(hash_algorithm<debug_hash>, "debug_hash should be a hashing algorithm");
	constexpr auto res1 = fnv1a_32{}(4);
	static_assert(
		has_update_hash_char<fnv1a_64>, "fnv1a_64 should have update_hash(char*, usize)"
	);
	constexpr auto res2 = fnv1a_64{}(std::array{ 1, 2, 3 });
	// constexpr auto res3 = debug_hash{}("hello", 5);
	// static_assert(std::convertible_to<decltype(res1), u32>, "fnv1a_32 should be convertible to u32");
	// static_assert(std::convertible_to<decltype(res2), u64>, "fnv1a_64 should be convertible to u64");
	// static_assert(std::convertible_to<decltype(res3), std::string>, "debug_hash should be convertible to std::string");
	// static_assert(static_cast<std::string>(res3).size() > 0, "debug_hash should return a non-empty string");

	hash<fnv1a_32> hasher;
	static_assert(hasher(1.f) != hasher(2.f), "hashes should differ");
	constexpr auto res3 = hasher(X{});
	constexpr auto res4 = hasher(S{});
	static_assert(res3 != res4, "hashes should differ");
	my_map::unordered_map<std::string, int> m;
	m["hello"] = 42;
	m["world"] = 7;
	assert(m["hello"] == 42);
	assert(m["world"] == 7);
}
