#include <base/ints.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hash.hpp>
#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <hashing/type_code.hpp>
#include <hashing/type_hash_code.hpp>
#include <hashing/type_unique_code.hpp>
#include <tester/tester.hpp>

#include <unordered_map>


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
concept check_hashRangeAsBytes = requires(T t) { internal::hashRangeAsBytes(Fnv1a_32{}, t); };

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

struct type2 {
	int         x{ 42 };
	bool        b{ true };
	std::string s{ "hello" };

	friend constexpr void addToHash(hashing::hash_algorithm auto& h, const type2& t) noexcept {
		addToHash(h, t.x);
		if (t.b) addToHash(h, t.s);
	}
};

struct type3 {
	int x{ 123 }, y{ 456 };
};

struct type4 {
	std::array<int, 2> a{ 123, 456 };
};

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
		TESTER_ADD_TEST(hashTest<SHA256>);
		TESTER_ADD_TEST(constexprTest);
		TESTER_ADD_TEST(defaultsTest<Fnv1a_32>);
		TESTER_ADD_TEST(defaultsTest<Fnv1a_64>);
		TESTER_ADD_TEST(defaultsTest<SHA256>);
		TESTER_ADD_TEST(uniqueCodeTest);
		TESTER_ADD_TEST(sha256Test);
		TESTER_ADD_TEST(stringLengthHashTest);
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
		constexpr auto res1 = Hash<Fnv1a_32, TypeCode<u32, false>>{}(4);
		constexpr auto arr  = std::array{ 1, 2, 3 };
		constexpr auto res2 = Hash<Fnv1a_64, TypeCode<u64, false>>{}(std::span{ arr });
		auto           res3 = [] {
            DebugHash dh;
            dh(std::as_bytes(std::span{ "hello" }));
            return dh.finalize().size();
		}();
		assertTrue(
			std::is_same_v<decltype(res1), const u32>, "Fnv1a_32's finalize() should return u32"
		);
		assertTrue(
			std::is_same_v<decltype(res2), const u64>, "Fnv1a_64's finalize() should return u64"
		);
		assertTrue(res3 > 0, "DebugHash should return a non-empty string");
		DebugHash dh;
		dh(std::as_bytes(std::span{ "hello 1234567890" }));

		Hash<DebugHash, TypeCode<>>{}(type3{});
		Hash<DebugHash>{}(type4{});
	}

	template<typename Alg>
	void hashTest() {
		Hash<Alg, TypeCode<u32, false>> hasher;
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
		constexpr auto r1 = Hash<Fnv1a_32, TypeCode<u32, false>>{}(876'543);
		constexpr auto r2 = Hash<Fnv1a_32, TypeCode<u32, false>>{}(std::string_view{ "hello" });
		constexpr auto r3 = TYPE_HASH_CODE<int>;
		constexpr auto r4 = TypeCode{ 23 };
		constexpr auto r5 = TypeCode<u16>{ 234 };
		constexpr auto r6 = Hash<Fnv1a_64, TypeCode<u32, false>>{}(S{});
		StatefulHash<Fnv1a_64, TypeCode<u32, false>> h;
		h(123, 345.f, std::string_view{ "hello" }, S{});

		return r1 + r2 + r3 + r4 + r5 + r6 + h.finalize();
	}

	void constexprTest() { [[maybe_unused]] constexpr auto res = constexprTestHelper(); }

	template<typename T>
	void defaultsTest() {
		StatefulHash<T, TypeCode<u16>> h;
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

	void sha256Test() {
		constexpr auto hash_value = hashing::StatefulHash<hashing::SHA256, TypeCode<u32, false>>{}(
										7,
										type2{},
										7,
										std::string{ "hello" },
										42,
										7,
										std::string{ "hello" },
										42,
										7,
										std::string{ "hello" },
										42,
										7,
										std::string{ "hello" },
										42,
										7,
										std::string{ "hello" },
										42
		)
		                                .finalize();


		const std::string expected_hash
			= "b2288f243a2cf2ce6c04b098b2f5ea7d140e961fc234cf55d08a42599847ad89";
		const std::string computed_hash = hash_value.toStringHex();

		assertTrue(
			computed_hash == expected_hash,
			"SHA256 hash does not match expected value.\nExpected: " + expected_hash
				+ "\nComputed: " + computed_hash
		);
	}

	void stringLengthHashTest() {
		auto hash_algo_1 = hashing::StatefulHash<hashing::SHA256, void>{};
		auto hash_algo_2 = hashing::StatefulHash<hashing::SHA256, void>{};
		
		hash_algo_1(std::string("ab"));
		hash_algo_1(std::string("c"));

		hash_algo_2(std::string("a"));
		hash_algo_2(std::string("bc"));

		ASSERT_TRUE(hash_algo_1.finalize() != hash_algo_2.finalize());
	}
};

TESTER_COMMON_MAIN("/src/common/hashing/tests/");
