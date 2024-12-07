#include <iostream>

// #include <tester/tester.hpp>
// #include <base/ints.hpp>
// #include <hashing/hash.hpp>

#include "../src/hashing/hash.hpp"
#include "../../tester/src/tester/tester.hpp"

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

namespace my_map {
	template<
		class Key,
		class T,
		class Hash  = hash<>,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>

	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

class HashingTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HashingTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hashCodeTest);
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(hashTest<fnv1a_32>);
		TESTER_ADD_TEST(hashTest<fnv1a_64>);
	}

private:
	void hashCodeTest() {
		assertTrue(type_hash_code<int> != type_hash_code<float>, "hash-codes should differ");
		assertTrue(type_hash_code<double> != type_hash_code<char>, "hash-codes should differ");
		assertTrue(type_hash_code<std::string> != type_hash_code<S>, "hash-codes should differ");
		assertTrue(type_hash_code<X> == type_hash_code<X>, "hash-code should be the same");
		assertTrue(
			std::convertible_to<decltype(type_hash_code<Y>), u32>,
			"hash-code should be convertible to u32"
		);
	}

	void hashingAlgorithmsTest() {
		assertTrue(hash_algorithm<fnv1a_32>, "fnv1a_32 should be a hashing algorithm");
		assertTrue(hash_algorithm<fnv1a_64>, "fnv1a_64 should be a hashing algorithm");
		assertTrue(hash_algorithm<debug_hash>, "debug_hash should be a hashing algorithm");
		constexpr auto res1 = fnv1a_32{}(4);
		assertTrue(
			has_update_hash_char<fnv1a_64>, "fnv1a_64 should have update_hash(char*, usize)"
		);
		constexpr auto res2 = fnv1a_64{}(std::array{ 1, 2, 3 });
		constexpr auto res3 = debug_hash{}("hello", 5);
		assertTrue(
			std::convertible_to<decltype(res1), u32>,
			"fnv1a_32 should be convertible to
			u32 "); assertTrue(std::convertible_to<decltype(res2), u64>, " fnv1a_64 should be
				convertible to u64
			"); assertTrue(std::convertible_to<decltype(res3), std::string>,
			"debug_hash should be convertible to std::string"
		);
		assertTrue(static_cast<std::string>(res3).size() > 0, "debug_hash should return a
		non-empty string");
	}

	template<typename Alg>
	void hashTest() {
		// hash<Alg> hasher;
		// assertTrue(hasher(1.f) != hasher(2.f), "hashes should differ");
		// constexpr auto res1 = hasher(X{});
		// constexpr auto res2 = hasher(S{});
		// assertTrue(res1 != res2, "hashes should differ");
		// my_map::unordered_map<std::string, int> m;
		// m["hello"] = 42;
		// m["world"] = 7;
		// assertTrue(m["hello"] == 42, "hello should be 42");
		// assertTrue(m["world"] == 7, "world should be 7");
	}
};

TESTER_COMMON_MAIN("/common/hashing/tests/");
