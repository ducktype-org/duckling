#include <base/types/ints.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hash.hpp>
#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <tester/tester.hpp>

#include <map>
#include <unordered_map>
#include <variant>
#include <vector>


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

struct Z: Y {
	int   x{};
	float y{};
	Y     z{};

public:
	friend constexpr auto hashDecompose(const Z& q) noexcept {
		return std::tie(q.x, q.y, q.z, getBase<Y>(q));
	}
};

struct S {
	int                x = 5;
	std::array<int, 2> y = { 1, 2 };
	bool               b{};
	float              z{ 7.0f };
	X                  m{ .x = 1, .y = 2, .z = 3.0f };
	Y                  n{ .f = 1.0f, .s = "hello" };
	Z                  o{ {}, 1, 2.0f, { .f = 1.0f, .s = "hello" } };

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

struct HashByRepresentation final {
	u64 x = 1;
	static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};
};

namespace my_map {
	using Hasher = decltype([](auto&& x) { return Hash<>{}(x).data.at(0); });

	template<
		class Key,
		class T,
		class Hash  = Hasher,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>

	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

template<typename T>
concept check_hashRangeAsBytes = requires(T t) { internal::hashRangeAsBytes(SHA256{}, t); };

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

struct algo {
	using result_type = u32;

	void operator()(std::span<const std::byte>) const {}

	[[nodiscard]]
	u32 finalize() const {
		return 0;
	}
};

struct algo2 {
	using result_type = u32;

	void operator()(std::span<const std::byte>) const {}

	void operator()(int) const {}

	[[nodiscard]]
	u32 finalize() const {
		return 0;
	}
};

class HashingTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HashingTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hashAlgorithmUtilsTest);
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(addToHashTest);
		TESTER_ADD_TEST(hashTest);
	}

private:
	void hashAlgorithmUtilsTest() {
		assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");
		static_assert(hash_algorithm<algo>, "algo should be a hashing algorithm");
		assertFalse(
			hash_algorithm<my_map::unordered_map<int, int>>,
			"unordered_map is not a hashing algorithm"
		);
		assertFalse(
			hash_algorithm<std::function<int(int)>>, "std::function is not a hashing algorithm"
		);
		assertFalse(
			hash_algorithm<std::function<void(std::span<char>)>>,
			"std::function should not be a hashing algorithm"
		);
		assertTrue(
			std::is_same_v<decltype(getBase<X>(type_with_bases{})), const X&>,
			"getBase<X>(type_with_bases{}) should return const X&"
		);


		assertTrue(
			internal::tuple_of_refs<std::tuple<int&, float&>>,
			"std::tuple<int&, float&> should be a tuple of references"
		);
		assertTrue(
			internal::tuple_of_refs<std::tuple<const int&, std::string&>>,
			"std::tuple<const int&, std::string&> should be a tuple of references"
		);
		assertFalse(
			internal::tuple_of_refs<std::vector<int>>,
			"std::vector<int> should not be a tuple of references"
		);
		assertFalse(
			internal::tuple_of_refs<std::tuple<int, float>>,
			"std::tuple<int, float> should not be a tuple of references"
		);
		assertFalse(
			internal::tuple_of_refs<std::tuple<int, float&>>,
			"std::tuple<int, float&> should not be a tuple of references"
		);
		assertFalse(
			internal::tuple_of_refs<std::array<int, 3>>,
			"std::array<int, 3> should not be a tuple of references"
		);

		assertTrue(internal::can_hashDecompose<X>, "X should be hashDecomposable");
		assertTrue(internal::can_hashDecompose<Y>, "Y should be hashDecomposable");
		assertTrue(internal::can_hashDecompose<Z>, "Z should be hashDecomposable");
		assertFalse(internal::can_hashDecompose<int>, "int should not be hashDecomposable");
		assertFalse(internal::can_hashDecompose<S>, "S should not be hashDecomposable");

		static_assert(
			internal::invocable_with_byte_span<SHA256>,
			"SHA256 should be invocable with a span of bytes"
		);

		SHA256 h1;
		internal::hashAsBytes(h1, 42.0f);
		internal::hashAsBytes(h1, std::array{ 1, 2, 3 });

		std::string s = "hello_long_string";
		assertTrue(
			internal::can_hash_range_as_bytes<SHA256, std::array<int, 3>>,
			"SHA256 should be able to hash as chars std::array<int, 3>"
		);

		assertTrue(
			internal::can_hash_range_as_bytes<SHA256, std::vector<int>>,
			"SHA256 should be able to hash as chars std::vector<int>"
		);

		assertFalse(
			internal::can_hash_range_as_bytes<SHA256, std::array<std::string, 3>>,
			"SHA256 should not be able to hash as chars std::array<std::string, 3>"
		);

		internal::hashRangeAsBytes(h1, std::array{ 1, 2, 3 });
		internal::hashRangeAsBytes(h1, s);
		internal::hashRangeAsBytes(h1, std::vector{ 1, 2, 3 });
		assertFalse(
			check_hashRangeAsBytes<std::map<int, int>>,
			"std::map<int, int> should not be hashable as chars"
		);
		assertFalse(
			check_hashRangeAsBytes<std::array<std::string, 3>>,
			"std::array<std::string, 3> should not be hashable as chars"
		);

		struct AnyType { std::uint64_t x; };
		static_assert(
			!internal::can_hash_by_representation<AnyType>,
			"AnyType should not be hashable by representation"
		);

		static_assert(
			internal::can_hash_by_representation<HashByRepresentation>,
			"HashByRepresentation should be hashable by representation"
		);
	}

	void hashingAlgorithmsTest() {
		SHA256              h2;
		constexpr std::span SP = "hello";
		assertTrue(
			std::is_same_v<SHA256::result_type, base::Bit256>,
			"SHA256::result_type should be base::Bit256"
		);
		assertTrue(
			std::is_same_v<DebugHash::result_type, std::string>,
			"DebugHash::result_type should be std::string"
		);
		std::string s = "qwertyuiopasdfghjk";
		h2(std::span{ reinterpret_cast<std::byte*>(s.data()), s.size() });
		h2(std::as_bytes(SP));
		constexpr auto ARR = std::array<char, 123>{};
		h2(std::as_bytes(std::span{ ARR }));
		SHA256 h4;
		assertTrue(
			h4.finalize() == h4.finalize(),
			"SHA256 should give the same values for .finalize() on the same state"
		);
		h4(std::as_bytes(SP));
		const auto& r = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
		h4(std::as_bytes(std::span{ r }));
		auto h5 = h4;
		assertTrue(h5.finalize() == h4.finalize(), "casted h5 should be equal to casted h4");
		h4(std::as_bytes(std::span{ s.data(), s.size() }));
		h5(std::as_bytes(std::span{ s.data(), s.size() }));
		h4(std::as_bytes(SP));
		h5(std::as_bytes(SP));
		assertTrue(h4.finalize() == h5.finalize(), "casted h4 should be equal to casted h5");

		DebugHash dh;
		dh(std::as_bytes(std::span{ s.data(), s.size() }));
		dh(std::as_bytes(SP));
		std::array<char, 16> array{};
		dh(std::as_bytes(std::span{ array }));
		Hash<DebugHash>{}(std::array<char, 16>{});
		dh(std::as_bytes(std::span{ SP.data(), SP.size() }));
		constexpr std::string_view SV2      = "qwertyuioplkjhgfdsazxcvbnm123456789098765432";
		constexpr auto             STR_SIZE = [&] {
            DebugHash d;
            internal::hashRangeAsBytes(d, SV2);
            return d.finalize().size();
		}();
		assertTrue(STR_SIZE == 187, "string should have 660 characters");
	}

	void addToHashTest() {
		SHA256 h;
		addToHash(h, Z{});
		addToHash(h, 42);
		addToHash(h, 42.0f);
		char*            ptr1 = nullptr;
		const int* const ptr2 = nullptr;
		S                s{};
		// auto             memptr = &S::y;
		addToHash(h, ptr1);
		addToHash(h, ptr2);
		// addToHash(h, memptr); // remove?
		addToHash(h, nullptr);
		addToHash(h, std::tuple{ 1, 2, 3 });
		addToHash(h, std::pair{ 1, 3 });
		addToHash(h, std::tuple<float, int, X, S>{ 1.0f, 2, X{}, S{} });
		std::array arr = std::array<S, 3>{ S{}, S{}, S{} };
		addToHash(h, arr);
		addToHash(h, HashByRepresentation{});

		std::map<int, int> m;
		m[1] = 2;
		m[3] = 4;
		m[5] = 6;
		// addToHash(h, m); // @future
		std::variant<int, float, std::string> v = 42;
		// addToHash(h, v); // @future

		addToHash(h, std::tuple{ 1, 2, 3 }, 123, 12.f, X{}, S{});
		addToHash(h, std::pair{ 1, 3 }, 123, 12.f, X{}, S{});
	}

	void hashTest() {
		SHA256 h2;
		addToHash(h2, S{});                       // S has addToHash overload
		assertTrue(internal::can_hashDecompose<Z>, "Z should be hashDecomposable");
		addToHash(h2, Z{});                       // Z has hashDecompose overload
		addToHash(h2, std::span{ "wertyuiop" });  // can hash directly
		addToHash(h2, 123.0f);                    // hashing floating point
		X* xptr = nullptr;
		addToHash(h2, xptr);                      // hashing pointer
		addToHash(h2, nullptr);                   // hashing nullptr
		auto range = std::vector{ 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }
		           | std::views::take(10);
		assertTrue(
			internal::can_hash_range_as_bytes<decltype(h2), decltype(range)>,
			"h2 should be able to hash range as chars"
		);
		addToHash(h2, range);  // hashing range as chars
		std::vector<Y> vec_y
			= { { .f = 1.0f, .s = "hello" }, { .f = 2.0f, .s = "world" }, { .f = 3.0f, .s = "!" } };
		assertTrue(
			std::ranges::contiguous_range<decltype(vec_y)>, "vec_y should be a contiguous range"
		);
		addToHash(h2, vec_y);  // hashing contiguous range
		my_map::unordered_map<int, int> m;
		m[1]                                    = 2;
		m[3]                                    = 4;
		m[5]                                    = 6;
		std::variant<int, float, std::string> v = 42;

		// addToHash(h2, v); // hashing std::variant // @future

		addToHash(h2, std::tuple{ 1, 2, 3 }, 123, 12.f, X{}, S{});

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
		m2[S{ .x = 1, .y = { 2, 3 } }] = 77;
		m2[S{ .x = 5, .y = { 987 } }]  = 33;
		m2[S{ .x = 1, .y = { 2, 3 } }] = 42;
		assertTrue(m2[S{ .x = 5, .y = { 987 } }] == 33, "m2[S{5}] should be 33");
		assertTrue(
			m2[S{ .x = 1, .y = { 2, 3 } }] == m2[S{}], "m2[S{1, 2, 3}] should be equal to m2[S{}]"
		);

		Hash{}(std::tuple{ 1, 2, 3 });
		[[maybe_unused]] auto var1 = hasher(std::tuple{ 1, 2, 3 });
		[[maybe_unused]] auto var2 = hasher(std::pair{ 1, 3 });
		[[maybe_unused]] auto var3 = hasher(std::tuple<float, int, X, S>{ 1.0f, 2, X{}, S{} });

		StatefulHash sh;
		sh(1);
		assertFalse(sh(123).finalize() == sh(123).finalize(), "stete should change");
		assertTrue(
			sh.finalize() == StatefulHash{}(1, 123, 123).finalize(), "state should be the same"
		);
		std::span sp = "dsfjsalfjfa salfjfalsdfj";
		sh("124241", sp, X{}, S{});
		sh(std::pair<int, S>{ 1, S{} });
		sh(type_with_bases{});
	}
};

TESTER_COMMON_MAIN("/src/common/hashing/tests/");
