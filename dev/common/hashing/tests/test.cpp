#include <iostream>

#include <tester/tester.hpp>
#include <base/ints.hpp>
#include <hashing/hash.hpp>

#include <stacktrace>

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

template<typename T>
concept check_hashRangeAsChars = requires(T t) { detail::hashRangeAsChars(Fnv1a_32{}, t); };

struct Check_1 { void updateHash(void*, usize) {} };
class Check_2 {	void updateHash(void*, usize) {} };
class Check_3 { protected: void updateHash(void*, usize) {} };
struct Check_4 { protected: void updateHash(std::string_view) {} };
struct Check_5 { protected: void updateHash(char*, usize) {} };
struct Check_6 { public: void updateHash(std::string_view) {} };

class HashingTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HashingTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hashCodeTest);
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(hashTest<Fnv1a_32>);
		TESTER_ADD_TEST(hashTest<Fnv1a_64>);
		TESTER_ADD_TEST(fullCoverageTest);
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
		// assertTrue(has_updateHash_char<Fnv1a_64>, "Fnv1a_64 should have updateHash(char*,
		// usize)"); constexpr auto res2 = Fnv1a_64{}(std::array{ 1, 2, 3 }); constexpr auto res3 =
		// static_cast<std::string>(DebugHash{}("hello", 5)).size(); assertTrue(
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

	void fullCoverageTest() {
		// type_hash_code_def.hpp
		assertTrue(
			std::is_same_v<TypeHashCode, TypeHashCodeBase<>>,
			"TypeHashCode should be TypeHashCodeBase<>"
		);
		TypeHashCodeBase<u64> thcb1;
		TypeHashCodeBase<u64> thcb2;
		assertTrue(
			std::is_same_v<TypeHashCodeBase<u64>::value_type, u64>,
			"TypeHashCodeBase should have value_type"
		);
		[[maybe_unused]] auto _ = static_cast<TypeHashCodeBase<u64>::value_type>(thcb1);
		assertTrue(thcb1 <= thcb2, "TypeHashCodeBase should be comparable");

		// hash_algorithm_utils.hpp
		assertTrue(hash_algorithm<Fnv1a_32>, "Fnv1a_32 should be a hashing algorithm");
		assertTrue(hash_algorithm<Fnv1a_64>, "Fnv1a_64 should be a hashing algorithm");
		assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");
		std::array<Fnv1a_32, 3> arr;
		assertFalse(hash_algorithm<decltype(arr)>, "array is not a hashing algorithm");
		assertFalse(
			hash_algorithm<my_map::unordered_map<int, int>>,
			"unordered_map is not a hashing algorithm"
		);
		assertFalse(
			hash_algorithm<std::function<int(int)>>, "std::function is not a hashing algorithm"
		);
		assertTrue(
			hash_algorithm<std::function<void(char*, usize)>>,
			"std::function could be a hashing algorithm"
		);

		assertTrue(detail::can_hash_directly<Fnv1a_32, int>, "Fnv1a_32 should be able to hash int");
		assertTrue(
			detail::can_hash_directly<Fnv1a_64, std::array<int, 3>>,
			"Fnv1a_64 should be able to hash std::array<int, 3>"
		);
		assertTrue(
			detail::can_hash_directly<Fnv1a_32, std::string>,
			"Fnv1a_32 should not be able to hash std::string"
		);
		assertFalse(
			detail::can_hash_directly<Fnv1a_64, float>, "Fnv1a_64 should not be able to hash float"
		);

		assertTrue(detail::can_stdhash<int>, "int should be hashable with std::hash");
		assertTrue(
			detail::can_stdhash<std::string>, "std::string should be hashable with std::hash"
		);
		assertFalse(detail::can_stdhash<Z>, "Z should not be hashable with std::hash");

		assertTrue(
			detail::tuple_of_refs<std::tuple<int&, float&>>,
			"std::tuple<int&, float&> should be a tuple of references"
		);
		assertTrue(
			detail::tuple_of_refs<std::tuple<const int&, std::string&>>,
			"std::tuple<const int&, std::string&> should be a tuple of references"
		);
		assertFalse(
			detail::tuple_of_refs<std::vector<int>>,
			"std::vector<int> should not be a tuple of references"
		);
		assertFalse(
			detail::tuple_of_refs<std::tuple<int, float>>,
			"std::tuple<int, float> should not be a tuple of references"
		);
		assertFalse(
			detail::tuple_of_refs<std::tuple<int, float&>>,
			"std::tuple<int, float&> should not be a tuple of references"
		);
		assertFalse(
			detail::tuple_of_refs<std::array<int, 3>>,
			"std::array<int, 3> should not be a tuple of references"
		);

		assertTrue(detail::can_hashDecompose<X>, "X should be hashDecomposable");
		assertTrue(detail::can_hashDecompose<Y>, "Y should be hashDecomposable");
		assertTrue(detail::can_hashDecompose<Z>, "Z should be hashDecomposable");
		assertFalse(detail::can_hashDecompose<int>, "int should not be hashDecomposable");
		assertFalse(detail::can_hashDecompose<S>, "S should not be hashDecomposable");

		detail::hashAsChars(Fnv1a_32{}, 42);
		Fnv1a_64 h1;
		detail::hashAsChars(h1, 42.0f);
		detail::hashAsChars(h1, std::array{ 1, 2, 3 });

		std::string s = "hello_long_string";
		assertTrue(
			detail::can_hash_range_as_chars<Fnv1a_32, std::array<int, 3>>,
			"Fnv1a_32 should be able to hash as chars std::array<int, 3>"
		);
		assertTrue(
			detail::can_hash_range_as_chars<Fnv1a_64, std::string>,
			"Fnv1a_64 should be able to hash as chars std::string"
		);
		assertTrue(
			detail::can_hash_range_as_chars<Fnv1a_32, std::vector<int>>,
			"Fnv1a_32 should be able to hash as chars std::vector<int>"
		);
		assertFalse(
			detail::can_hash_range_as_chars<Fnv1a_64, std::map<int, int>>,
			"Fnv1a_64 should not be able to hash as chars std::map<int, int>"
		);
		assertFalse(
			detail::can_hash_range_as_chars<Fnv1a_32, std::array<std::string, 3>>,
			"Fnv1a_32 should not be able to hash as chars std::array<std::string, 3>"
		);

		detail::hashRangeAsChars(h1, std::array{ 1, 2, 3 });
		detail::hashRangeAsChars(h1, s);
		detail::hashRangeAsChars(h1, std::vector{ 1, 2, 3 });
		assertFalse(
			check_hashRangeAsChars<std::map<int, int>>,
			"std::map<int, int> should not be hashable as chars"
		);
		assertFalse(
			check_hashRangeAsChars<std::array<std::string, 3>>,
			"std::array<std::string, 3> should not be hashable as chars"
		);

		// CallOverloads_utils.hpp
		assertFalse(has_updateHash_void<Check_1>, "Check_1 has public updateHash(void*, usize)");
		assertFalse(has_updateHash_void<Check_2>, "Check_2 has private updateHash(void*, usize)");
		assertTrue(has_updateHash_void<Check_3>, "Check_3 has protected updateHash(void*, usize)");
		assertFalse(has_updateHash_void<Check_4>, "Check_4 does not have updateHash(void*, usize)");
		assertFalse(has_updateHash_void<Check_5>, "Check_5 does not have updateHash(void*, usize)");
		assertFalse(has_updateHash_sv<Check_1>, "Check_1 does not have updateHash(std::string_view)");
		assertTrue(has_updateHash_sv<Check_4>, "Check_4 has protected updateHash(std::string_view)");
		assertFalse(has_updateHash_sv<Check_6>, "Check_6 does not have protected updateHash(std::string_view)");

		assertFalse(has_updateHash<Check_1>, "Check_1 does not have updateHash");
		assertFalse(has_updateHash<Check_2>, "Check_2 does not have updateHash");
		assertTrue(has_updateHash<Check_3>, "Check_3 has updateHash");
		assertTrue(has_updateHash<Check_4>, "Check_4 has updateHash");
		assertFalse(has_updateHash<Check_5>, "Check_5 does not have updateHash");
		assertFalse(has_updateHash<Check_6>, "Check_6 does not have updateHash");

		assertFalse(SHOULD_HASH_AS_HASH_CODE<Check_1, int>, "Check_1 is not a hash algorithm");
		assertFalse(SHOULD_HASH_AS_HASH_CODE<Fnv1a_32, int>, "int is not a TypeHashCode");
		assertFalse(SHOULD_HASH_AS_HASH_CODE<Fnv1a_64, TypeHashCode>, "Fnv1a_64 does not have addHashCode");
		assertTrue(SHOULD_HASH_AS_HASH_CODE<DebugHash, TypeHashCode>, "DebugHash has addHashCode");
		assertFalse(SHOULD_HASH_AS_HASH_CODE<DebugHash, int>, "int is not a TypeHashCode");

		// hashing_algorithms.hpp
		

	}
};

TESTER_COMMON_MAIN("/common/hashing/tests/");
