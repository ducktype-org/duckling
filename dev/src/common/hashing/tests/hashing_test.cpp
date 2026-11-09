// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/types/ints.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/component_hash.hpp>
#include <hashing/hash.hpp>
#include <hashing/hash_algorithm_utils.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <tester/tester.hpp>

#include <array>
#include <format>
#include <random>
#include <span>
#include <unordered_map>
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
	using Hasher = decltype([](auto&& x) { return Hash<>{}(x).data.at(0); });

	template<
		class Key,
		class T,
		class Hash  = Hasher,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>

	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

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

	static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};
};

template<typename From, typename To>
concept is_explicitly_convertible_to = requires(From f) { static_cast<To>(f); };

class HashingTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HashingTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(hashTest<SHA256>);
		TESTER_ADD_TEST(defaultsTest<SHA256>);
		TESTER_ADD_TEST(sha256Test);
		TESTER_ADD_TEST(sha256RuntimeTest);
		TESTER_ADD_TEST(pathHashTest);
		TESTER_ADD_TEST(byAddressTest);
	}

private:
	void hashingAlgorithmsTest() {
		assertTrue(hash_algorithm<DebugHash>, "DebugHash should be a hashing algorithm");
		assertTrue(hash_algorithm<SHA256>, "SHA256 should be a hashing algorithm");

		constexpr auto ARR = std::array{ 1, 2, 3 };
		SHA256         h;
		h(std::as_bytes(std::span{ ARR }));

		auto res3 = [] {
			DebugHash dh;
			dh(std::as_bytes(std::span{ "hello" }));
			return dh.finalize().size();
		}();

		assertTrue(res3 > 0, "DebugHash should return a non-empty string");

		DebugHash dh;
		dh(std::as_bytes(std::span{ "hello 1234567890" }));

		Hash<DebugHash>{}(type4{});
	}

	template<typename Alg>
	void hashTest() {
		Hash<Alg> hasher;

		assertTrue(hasher(1.f) != hasher(2.f), "hashes should differ");

		const auto res_1 = hasher(X{});
		const auto res_2 = hasher(S{});
		assertTrue(res_1 != res_2, "hashes should differ");

		my_map::unordered_map<std::string, int> m;
		m["hello"] = 42;
		m["world"] = 7;
		assertTrue(m["hello"] == 42, "hello should be 42");
		assertTrue(m["world"] == 7, "world should be 7");
	}

	template<typename T>
	void defaultsTest() {
		StatefulHash<T> h;
		h(123, 345.f, "hello", S{});

		Hash<T> h2;
		h2(123);
		h2(std::string_view{ "hello" });
	}

	void byAddressTest() {
		// raw pointers must not sneak into a hash through the "hash a contiguous range as
		// bytes" path either
		assertTrue(
			not internal::can_hash_range_as_bytes<SHA256, std::vector<int*>>,
			"a range of raw pointers should not be hashable as bytes"
		);

		std::array<int, 2> values{ 1, 2 };
		int* const         first  = values.data();
		int* const         second = values.data() + 1;

		Hash<SHA256> hasher;

		assertTrue(
			hasher(HashByAddress{ first }) == hasher(HashByAddress{ first }),
			"the same address should hash to the same value"
		);
		assertTrue(
			hasher(HashByAddress{ first }) != hasher(HashByAddress{ second }),
			"different addresses should hash to different values"
		);
		assertTrue(HashByAddress{ first }.get() == first, "get() should return the wrapped pointer");

		// only the address matters - neither the pointee nor the pointer's constness does
		const int* const also_second = second;
		values.at(1)                 = 42;
		assertTrue(
			hasher(HashByAddress{ second }) == hasher(HashByAddress{ also_second }),
			"the hash should depend only on the address"
		);
	}

	void sha256Test() {
		constexpr auto HASH_VALUE_SIMPLE
			= hashing::StatefulHash<hashing::SHA256>{}(std::byte{ 0x42 }).finalize();
		std::cerr << HASH_VALUE_SIMPLE << '\n' << HASH_VALUE_SIMPLE.toStringHex() << '\n';
		assertEqual(
			HASH_VALUE_SIMPLE.toStringHex(),
			"df7e70e5021544f4834bbee64a9e3789febc4be81470df629cad6ddb03320a5c",
			"SHA256 hash of a byte 0x42 does not match expected value:\nExpected:\n"
			"df7e70e5021544f4834bbee64a9e3789febc4be81470df629cad6ddb03320a5c\nComputed:\n"
				+ HASH_VALUE_SIMPLE.toStringHex()
		);

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

	/**
	 * Runtime hashing may use the hardware implementation (x86 SHA extensions), which has to give
	 * exactly the same results as the software one (used in constant evaluation).
	 */
	void sha256RuntimeTest() {
		auto runtime_hash = [](std::span<const std::byte> data) {
			hashing::SHA256 h;
			h(data);
			return h.finalize();
		};

		const std::string abc = "abc";
		assertEqual(
			runtime_hash(std::as_bytes(std::span(abc))).toStringHex(),
			hashing::StatefulHash<hashing::SHA256>{}(std::span(abc)).finalize().toStringHex(),
			"runtime and compile time path should agree"
		);

		std::mt19937_64        rng(42);
		std::vector<std::byte> data;
		for (usize len = 0; len <= 300; len++) {
			data.resize(len);
			for (auto& b: data) b = static_cast<std::byte>(rng());

			// Reference: the software block transform
			hashing::SHA256 reference;
#if DUCK_SHA256_HARDWARE_X86
			const bool prev                        = hashing::internal::sha256_use_hardware;
			hashing::internal::sha256_use_hardware = false;
			reference(std::span<const std::byte>(data));
			const auto expected                    = reference.finalize();
			hashing::internal::sha256_use_hardware = prev;
#else
			reference(std::span<const std::byte>(data));
			const auto expected = reference.finalize();
#endif
			// Also feed the data in two parts to exercise buffering.
			hashing::SHA256 split;
			split(std::span<const std::byte>(data).first(len / 3));
			split(std::span<const std::byte>(data).subspan(len / 3));

			assertEqual(
				runtime_hash(data).toStringHex(),
				expected.toStringHex(),
				"SHA256 implementations disagree for length " + std::to_string(len)
			);
			assertEqual(
				split.finalize().toStringHex(),
				expected.toStringHex(),
				"SHA256 of split input disagrees for length " + std::to_string(len)
			);
		}
#if DUCK_SHA256_HARDWARE_X86
		std::cerr << "SHA256 hardware implementation used: "
				  << (hashing::internal::sha256_use_hardware ? "yes" : "no") << "\n";
#endif
	}

	/**
	 * PathHash has to hash exactly the same bytes as ComponentHash (PST element hashes are built on
	 * top of the partial state and are stored across compilations).
	 */
	void pathHashTest() {
		using hashing::ComponentHash;
		using hashing::PathHash;

		const ComponentHash root(base::StrID("root"));
		ComponentHash       component = root;
		PathHash            path      = root;
		assertEqual(path.finalize().toStringHex(), component.hash.toStringHex(), "root");

		for (usize i = 0; i < 40; i++) {
			if (i % 3 == 0) {
				component = ComponentHash(component, std::format("[{}]", i * 997));
				path      = PathHash(path, PathHash::Index{ i * 997 });
			} else if (i % 3 == 1) {
				component = ComponentHash(component, std::string_view("by_symbol"));
				path      = PathHash(path, std::string_view("by_symbol"));
			} else {
				component = ComponentHash(component, std::string_view(""));
				path      = PathHash(path, std::string_view(""));
			}
			assertEqual(
				path.finalize().toStringHex(),
				component.hash.toStringHex(),
				"PathHash differs from ComponentHash at step " + std::to_string(i)
			);
		}
	}
};

TESTER_COMMON_MAIN("/src/common/hashing/tests/");
