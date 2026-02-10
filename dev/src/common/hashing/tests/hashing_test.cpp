#include <base/types/ints.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hash.hpp>
#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>
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
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(hashTest<Fnv1a_32>);
		TESTER_ADD_TEST(hashTest<Fnv1a_64>);
		TESTER_ADD_TEST(hashTest<SHA256>);
		TESTER_ADD_TEST(constexprTest);
		TESTER_ADD_TEST(defaultsTest<Fnv1a_32>);
		TESTER_ADD_TEST(defaultsTest<Fnv1a_64>);
		TESTER_ADD_TEST(defaultsTest<SHA256>);
		TESTER_ADD_TEST(sha256Test);
	}

private:
	void hashingAlgorithmsTest() {
		assertTrue(hash_algorithm<Fnv1a_32>, "Fnv1a_32 should be a hashing algorithm");
		assertTrue(hash_algorithm<Fnv1a_64>, "Fnv1a_64 should be a hashing algorithm");
		assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");

		constexpr auto RES1 = Hash<Fnv1a_32>{}(4);

		constexpr auto ARR  = std::array{ 1, 2, 3 };
		constexpr auto RES2 = Hash<Fnv1a_64>{}(std::span{ ARR });

		auto res3 = [] {
			DebugHash dh;
			dh(std::as_bytes(std::span{ "hello" }));
			return dh.finalize().size();
		}();

		assertTrue(
			std::is_same_v<decltype(RES1), const u32>, "Fnv1a_32's finalize() should return u32"
		);
		assertTrue(
			std::is_same_v<decltype(RES2), const u64>, "Fnv1a_64's finalize() should return u64"
		);
		assertTrue(res3 > 0, "DebugHash should return a non-empty string");

		DebugHash dh;
		dh(std::as_bytes(std::span{ "hello 1234567890" }));

		Hash<DebugHash>{}(type4{});
	}

	template<typename Alg>
	void hashTest() {
		Hash<Alg> hasher;

		assertTrue(hasher(1.f) != hasher(2.f), "hashes should differ");

		constexpr auto RES1 = hasher(X{});
		constexpr auto RES2 = hasher(S{});
		assertTrue(RES1 != RES2, "hashes should differ");

		my_map::unordered_map<std::string, int> m;
		m["hello"] = 42;
		m["world"] = 7;
		assertTrue(m["hello"] == 42, "hello should be 42");
		assertTrue(m["world"] == 7, "world should be 7");
	}

	[[nodiscard]]
	static constexpr u64 constexprTestHelper() {
		constexpr auto R1 = Hash<Fnv1a_32>{}(876'543);

		constexpr auto R2 = Hash<Fnv1a_32>{}(std::string_view{ "hello" });

		constexpr auto R3 = Hash<Fnv1a_64>{}(S{});

		StatefulHash<Fnv1a_64> h;
		h(123, 345.f, std::string_view{ "hello" }, S{});

		return R1 + R2 + R3 + h.finalize();
	}

	void constexprTest() { [[maybe_unused]] constexpr auto RES = constexprTestHelper(); }

	template<typename T>
	void defaultsTest() {
		StatefulHash<T> h;
		h(123, 345.f, "hello", S{});

		Hash<T> h2;
		h2(123);
		h2(std::string_view{ "hello" });
	}

	void sha256Test() {
		constexpr auto HASH_VALUE = hashing::StatefulHash<hashing::SHA256>{}(
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
			= "cc29a5e32052f1e78ce5933758b457e9829c84322bfa1e8e2794ae1456a274a0";
		const std::string computed_hash = HASH_VALUE.toStringHex();

		assertTrue(
			computed_hash == expected_hash,
			"SHA256 hash does not match expected value.\nExpected: " + expected_hash
				+ "\nComputed: " + computed_hash
		);
	}
};

TESTER_COMMON_MAIN("/src/common/hashing/tests/");
