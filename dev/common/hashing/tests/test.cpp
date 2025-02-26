#include <unordered_map>
#include <iostream>
#include <variant>
#include <vector>

#include <tester/tester.hpp>
#include <base/ints.hpp>

#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <hashing/type_unique_code.hpp>
#include <hashing/type_hash_code.hpp>
#include <hashing/add_to_hash.hpp>
#include <hashing/type_code.hpp>
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
	X                  m{ .x = 1, .y = 2, .z = 3.0f };
	Y                  n{ .f = 1.0f, .s = "hello" };
	Z                  o{ .x = 1, .y = 2.0f, .z = { .f = 1.0f, .s = "hello" } };

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

struct type_with_bases: X, S {
	int x{ 123 }, y{ 456 };

	friend constexpr auto hashDecompose(const type_with_bases& t) {
		using namespace hashing;
		return std::tie(getBase<X>(t), getBase<S>(t), t.x, t.y);
	}
};

struct type3 {
	int x{ 123 }, y{ 456 };
};

struct type4 {
	std::array<int, 2> a{ 123, 456 };
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
		TESTER_ADD_TEST(constexprTest);
		TESTER_ADD_TEST(defaultsTest<Fnv1a_32>);
		TESTER_ADD_TEST(defaultsTest<Fnv1a_64>);
		TESTER_ADD_TEST(uniqueCodeTest);
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
		constexpr auto arr  = std::array{ 1, 2, 3 };
		constexpr auto res2 = Fnv1a_64{}(std::span{ arr });
		constexpr auto res3 = [] {
			DebugHash dh;
			dh(std::span{ "hello" });
			return static_cast<std::string>(dh).size();
		}();
		assertTrue(
			is_explicitly_convertible_to<decltype(res1), u32>,
			"Fnv1a_32 should be convertible to u32"
		);
		assertTrue(
			is_explicitly_convertible_to<decltype(res2), u64>,
			" Fnv1a_64 should be convertible to u64"
		);
		assertTrue(res3 > 0, "DebugHash should return a non-empty string");
		DebugHash dh;
		dh(std::span{ "hello 1234567890" });

		Hash<DebugHash, TypeCode>{}(type3{});
		Hash<DebugHash>{}(type4{});
	}

	template<typename Alg>
	void hashTest() {
		Hash<Alg> hasher;
		assertTrue(hasher(1.f) != hasher(2.f), "hashes should differ");
		constexpr auto res1 = hasher(X{});
		constexpr auto res2 = hasher(S{});
		assertTrue(res1 != res2, "hashes should differ");
		my_map::unordered_map<std::string, int> m;
		m["hello"] = 42;
		m["world"] = 7;
		assertTrue(m["hello"] == 42, "hello should be 42");
		assertTrue(m["world"] == 7, "world should be 7");
	}

	[[nodiscard]]
	static constexpr u64 constexprTestHelper() {
		constexpr auto         r1 = Hash<Fnv1a_32>{}(876'543);
		constexpr auto         r2 = Hash<Fnv1a_32>{}(std::string_view{ "hello" });
		constexpr auto         r3 = TYPE_HASH_CODE<int>;
		constexpr auto         r4 = TypeCode{ 23 };
		constexpr auto         r5 = TypeCodeBase<u16>{ 234 };
		constexpr auto         r6 = Hash{}(S{});
		StatefulHash<Fnv1a_64> h;
		h(123, 345.f, std::string_view{ "hello" }, S{});

		return r1 + r2 + r3 + r4 + r5 + r6 + static_cast<u64>(h);
	}

	void constexprTest() { [[maybe_unused]] constexpr auto res = constexprTestHelper(); }

	template<typename T>
	void defaultsTest() {
		StatefulHash<T, TypeCodeBase<u16>> h;
		h(123, 345.f, "hello", S{});
		static_assert(requires { typename decltype(h)::TypeCode_value_type; });
		Hash<T, void> h2;
		h2(123);
		h2(std::string_view{ "hello" });
		static_assert(not requires { typename decltype(h2)::TypeCode_value_type; });
	}

	void uniqueCodeTest() {
		auto id1 = TYPE_UNIQUE_CODE<int>;
		auto id2 = TYPE_UNIQUE_CODE<int>;
		assertTrue(id1 == id2, "unique codes should be the same");
		auto id3 = TYPE_UNIQUE_CODE<float>;
		assertTrue(id1 != id3, "unique codes should differ");
		auto id4 = TYPE_UNIQUE_CODE<S>;
		assertTrue(id1 != id4, "unique codes should differ");
	}
};

TESTER_COMMON_MAIN("/common/hashing/tests/");
