#include <iostream>
#include <unordered_map>
#include <variant>
#include <vector>

#include <tester/tester.hpp>
#include <base/ints.hpp>
#include <hashing/hash.hpp>
#include <hashing/hashing_algorithms.hpp>

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

	friend bool operator<(const S& lhs, const S& rhs) noexcept { return lhs.x < rhs.x; }

	friend bool operator==(const S& lhs, const S& rhs) noexcept { return lhs.x == rhs.x; }
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

struct Check_1 {
	void updateHash(void*, usize) {}
};

class Check_2 {
	void updateHash(void*, usize) {}
};

class Check_3 {
protected:
	void updateHash(void*, usize) {}
};

struct Check_4 {
protected:
	void updateHash(std::string_view) {}
};

struct Check_5 {
protected:
	void updateHash(char*, usize) {}
};

struct Check_6 {
public:
	void updateHash(std::string_view) {}
};

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
		assertTrue(TYPE_HASH_CODE<int> != TYPE_HASH_CODE<float>, "hash-codes should differ");
		assertTrue(TYPE_HASH_CODE<double> != TYPE_HASH_CODE<char>, "hash-codes should differ");
		assertTrue(TYPE_HASH_CODE<std::string> != TYPE_HASH_CODE<S>, "hash-codes should differ");
		assertTrue(TYPE_HASH_CODE<X> == TYPE_HASH_CODE<X>, "hash-code should be the same");
		assertTrue(
			is_explicitly_convertible_to<decltype(TYPE_HASH_CODE<Y>), u32>,
			"hash-code should be convertible to u32"
		);
	}

	void hashingAlgorithmsTest() {
		assertTrue(hash_algorithm<Fnv1a_32>, "Fnv1a_32 should be a hashing algorithm");
		assertTrue(hash_algorithm<Fnv1a_64>, "Fnv1a_64 should be a hashing algorithm");
		assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");
		constexpr auto res1 = Fnv1a_32{}(4);
		assertTrue(has_updateHash_void<Fnv1a_64>, "Fnv1a_64 should have updateHash(char*, usize)");
		constexpr auto res2 = Fnv1a_64{}(std::array{ 1, 2, 3 });
		constexpr auto res3
			= static_cast<std::string>(DebugHash{}(std::string_view{ "hello" })).size();
		assertTrue(
			is_explicitly_convertible_to<decltype(res1), u32>,
			"Fnv1a_32 should be convertible to u32"
		);
		assertTrue(
			is_explicitly_convertible_to<decltype(res2), u64>,
			" Fnv1a_64 should be convertible to u64"
		);
		assertTrue(res3 > 0, "DebugHash should return a non-empty string");
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
		std::array<Fnv1a_32, 3> fnv_arr;
		assertFalse(hash_algorithm<decltype(fnv_arr)>, "array is not a hashing algorithm");
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
		assertFalse(
			has_updateHash_sv<Check_1>, "Check_1 does not have updateHash(std::string_view)"
		);
		assertTrue(
			has_updateHash_sv<Check_4>, "Check_4 has protected updateHash(std::string_view)"
		);
		assertFalse(
			has_updateHash_sv<Check_6>,
			"Check_6 does not have protected updateHash(std::string_view)"
		);

		assertFalse(has_updateHash<Check_1>, "Check_1 does not have updateHash");
		assertFalse(has_updateHash<Check_2>, "Check_2 does not have updateHash");
		assertTrue(has_updateHash<Check_3>, "Check_3 has updateHash");
		assertTrue(has_updateHash<Check_4>, "Check_4 has updateHash");
		assertFalse(has_updateHash<Check_5>, "Check_5 does not have updateHash");
		assertFalse(has_updateHash<Check_6>, "Check_6 does not have updateHash");

		assertFalse(SHOULD_HASH_AS_HASH_CODE<Check_1, int>, "Check_1 is not a hash algorithm");
		assertFalse(SHOULD_HASH_AS_HASH_CODE<Fnv1a_32, int>, "int is not a TypeHashCode");
		assertFalse(
			SHOULD_HASH_AS_HASH_CODE<Fnv1a_64, TypeHashCode>, "Fnv1a_64 does not have addHashCode"
		);
		assertTrue(SHOULD_HASH_AS_HASH_CODE<DebugHash, TypeHashCode>, "DebugHash has addHashCode");
		assertFalse(SHOULD_HASH_AS_HASH_CODE<DebugHash, int>, "int is not a TypeHashCode");

		// hashing_algorithms.hpp
		Fnv1a_32                   h2;
		constexpr std::string_view sv = "hello";
		assertTrue(
			std::is_same_v<Fnv1a_32::result_type, u32>, "Fnv1a_32::result_type should be u32"
		);
		assertTrue(
			std::is_same_v<Fnv1a_64::result_type, u64>, "Fnv1a_64::result_type should be u64"
		);
		assertTrue(
			std::is_same_v<DebugHash::result_type, std::string>,
			"DebugHash::result_type should be std::string"
		);
		h2(static_cast<const void*>(sv.data()), sv.size());
		h2(static_cast<void*>(s.data()), s.size());
		h2(sv);
		h2(std::array<char, 123>{});
		Fnv1a_64              h3, h4{ 14'695'981'039'346'656'037ull };
		[[maybe_unused]] auto discard = static_cast<decltype(h3)::result_type>(h3);
		assertTrue(
			static_cast<u64>(h4) == static_cast<u64>(h4),
			"h3 and h4 should have been initialized with the same value"
		);
		h4(sv);
		const auto& r = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
		h4(r);
		auto h5 = h4;
		assertTrue(
			static_cast<u64>(h5) == static_cast<u64>(h4), "casted h5 should be equal to casted h4"
		);
		h4(s.data(), s.size());
		h5(s.data(), s.size());
		h4(sv);
		h5(sv);
		h4(static_cast<const void*>(sv.data()), sv.size());
		h5(static_cast<const void*>(sv.data()), sv.size());
		assertTrue(
			static_cast<u64>(h4) == static_cast<u64>(h5), "casted h4 should be equal to casted h5"
		);

		DebugHash dh;
		dh(s.data(), s.size());
		dh(sv);
		dh(std::array<char, 16>{});
		dh(static_cast<const void*>(sv.data()), sv.size());
		constexpr std::string_view sv2      = "qwertyuioplkjhgfdsazxcvbnm123456789098765432";
		constexpr auto             str_size = static_cast<std::string>(DebugHash{}(sv2)).size();
		assertTrue(str_size == 187, "string should have 187 characters");

		assertTrue(
			hash_algorithm<default_hash_algorithm_for<u32>>,
			"default_hash_algorithm_for<u32> should be a hashing algorithm"
		);
		assertTrue(
			hash_algorithm<default_hash_algorithm_for<u64>>,
			"default_hash_algorithm_for<u64> should be a hashing algorithm"
		);

		// hash.hpp
		addToHash(h2, S{});  // S has addToHash overload
		assertTrue(detail::can_hashDecompose<Z>, "Z should be hashDecomposable");
		addToHash(h2, Z{});  // Z has hashDecompose overload
		assertTrue(
			detail::can_hash_directly<decltype(h2), std::string_view>,
			"h2 should be able to hash std::string_view"
		);
		addToHash(h2, std::string_view{ "wertyuiop" });  // can hash directly
		addToHash(h2, 123.0f);                           // hashing floating point
		addToHash(h2, &sv);                              // hashing pointer
		addToHash(h2, nullptr);                          // hashing nullptr
		auto range = std::vector{ 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }
		           | std::views::take(10);
		assertTrue(
			detail::can_hash_range_as_chars<decltype(h2), decltype(range)>,
			"h2 should be able to hash range as chars"
		);
		addToHash(h2, range);  // hashing range as chars
		std::vector<Y> vec_y = { { 1.0f, "hello" }, { 2.0f, "world" }, { 3.0f, "!" } };
		assertTrue(
			std::ranges::contiguous_range<decltype(vec_y)>, "vec_y should be a contiguous range"
		);
		addToHash(h2, vec_y);  // hashing contiguous range
		my_map::unordered_map<int, int> m;
		m[1] = 2;
		m[3] = 4;
		m[5] = 6;
		assertTrue(
			detail::can_hash_range_with_unspecified_order<decltype(h2), decltype(m)>,
			"h2 should be able to hash range with unspecified order"
		);
		addToHash(h2, m);  // hashing range with unspecified order
		std::variant<int, float, std::string> v = 42;
		assertTrue(detail::can_stdhash<decltype(v)>, "v should be hashable with std::hash");
		addToHash(h2, v);  // hashing std::variant

		Hash hasher;
		auto res1 = hasher(1.f);
		auto res2 = hasher(2.f);
		assertTrue(res1 != res2, "hashes should differ");
		auto res3 = hasher(X{});
		auto res4 = hasher(Y{});
		assertTrue(res3 != res4, "hashes should differ");
		assertTrue(hasher(S{}) == hasher(S{}), "hashes should be the same");
		my_map::unordered_map<S, int> m2;
		m2[S{}] = 42;
		assertTrue(m2[S{}] == 42, "m2[S{}] should be 42");
		m2[S{ 1, { 2, 3 } }] = 77;
		m2[S{ 5, { 987 } }]  = 33;
		m2[S{ 1, { 2, 3 } }] = 42;
		assertTrue(m2[S{ 5, { 987 } }] == 33, "m2[S{5}] should be 33");
		assertTrue(m2[S{ 1, { 2, 3 } }] == m2[S{}], "m2[S{1, 2, 3}] should be equal to m2[S{}]");

		StatefulHash sh;
		sh(1);
		assertFalse(sh(123) == sh(123), "stete should change");
		assertTrue(static_cast<u64>(sh) == StatefulHash{}(1, 123, 123), "state should be the same");
		sh("124241", sv, X{}, S{});
	}
};

TESTER_COMMON_MAIN("/common/hashing/tests/");
