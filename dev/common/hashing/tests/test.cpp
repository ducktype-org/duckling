#include <iostream>

#include <tester/tester.hpp>
#include <base/ints.hpp>
#include <hashing/hash.hpp>

using namespace hashing;

struct X {
	int   x{};
	int   y{};
	float z{};

public:
	friend constexpr auto hashDecompose(const X& q) noexcept { return std::tie(q.x, q.y, q.z); }
};

struct Y {
	float       f{};
	std::string s{};

public:
	friend constexpr auto hashDecompose(const Y& q) noexcept { return std::tie(q.f, q.s); }
};

struct Z {
	int   x{};
	float y{};
	Y     z{};

public:
	friend constexpr auto hashDecompose(const Z& q) noexcept { return std::tie(q.x, q.y, q.z); }
};

struct S {
	int                x = 5;
	std::array<int, 2> y = { 1, 2 };
	bool               b{};
	float              z{ 7.0f };
	X                  m{ 1, 2, 3.0f };
	Y                  n{ 1.0f, "hello" };
	Z                  o{ 1, 2.0f, { 1.0f, "hello" } };

public:
	friend constexpr void addToHash(hash_algorithm auto& h, const S& s) noexcept {
		addToHash(h, s.x);
		addToHash(h, s.y);
		if (s.b) addToHash(h, s.z);
		addToHash(h, s.m);
		addToHash(h, s.n);
		addToHash(h, s.o);
	}
};

namespace my_map {
	template<
		class Key,
		class T,
		class Hash  = Hash<>,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>

	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

template<typename From, typename To>
concept is_explicitly_convertible_to = requires(From f) { static_cast<To>(f); };

class HashingTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HashingTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hashCodeTest);
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(hashTest<Fnv1a_32>);
		TESTER_ADD_TEST(hashTest<Fnv1a_64>);
		TESTER_ADD_TEST(betterCoverage);
	}

private:
	void hashCodeTest() {
		// assertTrue(TYPE_HASH_CODE<int> != TYPE_HASH_CODE<float>, "hash-codes should differ");
		// assertTrue(TYPE_HASH_CODE<double> != TYPE_HASH_CODE<char>, "hash-codes should differ");
		// assertTrue(TYPE_HASH_CODE<std::string> != TYPE_HASH_CODE<S>, "hash-codes should differ");
		// assertTrue(TYPE_HASH_CODE<X> == TYPE_HASH_CODE<X>, "hash-code should be the same");
		// assertTrue(
		// 	is_explicitly_convertible_to<decltype(TYPE_HASH_CODE<Y>), u32>,
		// 	"hash-code should be convertible to u32"
		// );
	}

	void hashingAlgorithmsTest() {
		// assertTrue(hash_algorithm<Fnv1a_32>, "Fnv1a_32 should be a hashing algorithm");
		// assertTrue(hash_algorithm<Fnv1a_64>, "Fnv1a_64 should be a hashing algorithm");
		// assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");
		// constexpr auto res1 = Fnv1a_32{}(4);
		// assertTrue(has_updateHash_char<Fnv1a_64>, "Fnv1a_64 should have updateHash(char*, usize)");
		// constexpr auto res2 = Fnv1a_64{}(std::array{ 1, 2, 3 });
		// constexpr auto res3 = static_cast<std::string>(DebugHash{}("hello", 5)).size();
		// assertTrue(
		// 	is_explicitly_convertible_to<decltype(res1), u32>,
		// 	"Fnv1a_32 should be convertible to u32"
		// );
		// assertTrue(
		// 	is_explicitly_convertible_to<decltype(res2), u64>,
		// 	" Fnv1a_64 should be convertible to u64"
		// );
		// assertTrue(res3 > 0, "DebugHash should return a non-empty string");
	}

	template<typename Alg>
	void hashTest() {
		// Hash<Alg> hasher;
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

	void betterCoverage() {
		// type_hash_code_def.hpp
		assertTrue(std::is_same_v<TypeHashCode, TypeHashCodeBase<>>, "TypeHashCode should be TypeHashCodeBase<>"); 
		TypeHashCodeBase<u64> thcb1;
		TypeHashCodeBase<u64> thcb2;
		assertTrue(std::is_same_v<TypeHashCodeBase<u64>::value_type, u64>, "TypeHashCodeBase should have value_type");
		static_cast<TypeHashCodeBase<u64>::value_type>(thcb1);
		assertTrue(thcb1 <= thcb2, "TypeHashCodeBase should be comparable");
		
		// hash_algorithm_utils.hpp
		assertTrue(hash_algorithm<Fnv1a_32>, "Fnv1a_32 should be a hashing algorithm");
		assertTrue(hash_algorithm<Fnv1a_64>, "Fnv1a_64 should be a hashing algorithm");
		assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");

		
	}
};

TESTER_COMMON_MAIN("/common/hashing/tests/");
