#include <iostream>

#include <tester/tester.hpp>
#include <base/ints.hpp>
#include <hashing/hash.hpp>

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
	friend constexpr void add_to_hash(hashing::hash_algorithm auto& h, const S& s) noexcept {
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
		class Hash  = hashing::hash<>,
		class Pred  = std::equal_to<Key>,
		class Alloc = std::allocator<std::pair<const Key, T>>>

	using unordered_map = std::unordered_map<Key, T, Hash, Pred, Alloc>;
}

class HashingTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HashingTest


	using namespace hashing;

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(hashCodeTest);
		TESTER_ADD_TEST(hashingAlgorithmsTest);
		TESTER_ADD_TEST(hashTest<fnv1a_32>);
		TESTER_ADD_TEST(hashTest<fnv1a_64>);
	}

private:
	void hashCodeTest() {
		assertTrue(type_hash_code<int> != type_hash_code<float>);
		assertTrue(type_hash_code<double> != type_hash_code<char>);
		assertTrue(type_hash_code<std::string> != type_hash_code<S>);
		assertTrue(type_hash_code<X> == type_hash_code<X>);
		assertTrue(std::convertible_to<u32>(type_hash_code<Y>));
	}

	void hashingAlgorithmsTest() {
		assertTrue(hashing_algorithm<fnv1a_32>);
		assertTrue(hashing_algorithm<fnv1a_64>);
		assertTrue(hashing_algorithm<debug_hash>);
		constexpr auto res1 = fnv1a_32{}(4);
		constexpr auto res2 = fnv1a_64{}(42.0);
		constexpr auto res3 = debug_hash{}("hello", 5);
		assertTrue(std::convertible_to<u32>(res1));
		assertTrue(std::convertible_to<u64>(res2));
		assertTrue(std::convertible_to<std::string>(res3));
		assertTrue(static_cast<std::string>(res3).size() > 0);
	}

	template<typename Alg>
	void hashTest() {
		hash<Alg> hasher;
		assertTrue(hasher(1.f) != hasher(2.f));
		constexpr auto res1 = hasher(X{});
		constexpr auto res2 = hasher(S{});
		assertTrue(res1 != res2);
		my_map::unordered_map<std::string, int> m;
		m["hello"] = 42;
		m["world"] = 7;
		assertTrue(m["hello"] == 42);
		assertTrue(m["world"] == 7);
	}
};

TESTER_COMMON_MAIN("/common/hashing/tests/");


// int main() {
// 	using namespace hashing;

// 	constexpr auto res  = fnv1a_32{}(4);
// 	constexpr auto res2 = static_cast<u32>(fnv1a_32{}(42)(43));

// 	std::cout << typeid(res).name() << ' ' << typeid(res2).name() << std::endl;

// 	fnv1a_32 h;

// 	h("hello", 5);

// 	std::cout << "1 hash: " << static_cast<u32>(h) << std::endl;

// 	int x = 42;
// 	h(std::addressof(x), sizeof(x));
// 	h(x);
// 	std::cout << "2 hash: " << static_cast<u32>(h) << std::endl;

// 	h(42);
// 	std::cout << "3 hash: " << static_cast<u32>(h) << std::endl;

// 	static_assert(hash_algorithm<fnv1a_32>);
// 	static_assert(hash_algorithm<fnv1a_64>);
// 	static_assert(hash_algorithm<debug_hash>);

// 	hash<fnv1a_64> hasher;
// 	add_to_hash(hasher, 1.f);

// 	S    s{};
// 	auto a = hasher(s);
// 	std::cout << "4 hash: " << a << std::endl;

// 	X    x1{ 1, 2, 3.0f };
// 	auto b = hasher(x1);
// 	std::cout << "5 hash: " << b << std::endl;

// 	std::string t  = "hello";
// 	S 		 s2 = { 5, { 1, 2 }, true, 7.0f, { 1, 2, 3.0f }, { 1.0f, "hello" }, { 1, 2.0f, { 1.0f,
// "hello" } }}; 	X 		 x2 = { 1, 2, 3.0f };

// 	std::cout << "6 hash: " << hasher(t) << std::endl;
// 	std::cout << "7 hash: " << hasher(s2) << std::endl;
// 	std::cout << "8 hash: " << hasher(x2) << std::endl;

// 	constexpr auto res3 = fnv1a_32{}("hello", 5);

// 	std::cout << "9 hash: " << static_cast<u32>(res3) << std::endl;


// 	std::cout << type_hash_code<int> << ' ' << type_hash_code<float> << ' '
// 			  << type_hash_code<double> << ' ' << type_hash_code<char> << ' '
// 			  << type_hash_code<std::string> << ' ' << type_hash_code<S> << ' ' << type_hash_code<X>
// 			  << std::endl;
// 	std::cout << type_hash_code<int> << ' ' << type_hash_code<float> << ' '
// 			  << type_hash_code<double> << ' ' << type_hash_code<char> << ' '
// 			  << type_hash_code<std::string> << ' ' << type_hash_code<S> << ' ' << type_hash_code<X>
// 			  << std::endl;

// 	static_assert(hash_algorithm<debug_hash>);
// 	static_assert(has_update_hash_void<debug_hash>);
// 	static_assert(has_update_hash_char<debug_hash>);

// 	stateful_hash<debug_hash> dh;
// 	dh("hello");
// 	dh(42);
// 	dh(42.f);
// 	dh(7);
// 	dh(42.0);
// 	std::cout << static_cast<std::string>(dh) << std::endl;

// 	std::cout << "variadic:\n";
// 	stateful_hash<debug_hash> dh2;
// 	std::cout << dh2("hello", 42, 42.f, 7, 42.0) << std::endl;
// 	std::cout << "inline variadic fnv1a: " << stateful_hash{}("hello", 42, 42.f, 7, 42.0) <<
// std::endl;

// 	constexpr auto res4 = stateful_hash{}("hello", 42, 42.f, 7, 42.0);
// 	std::cout << "constexpr: " << static_cast<u32>(res4) << std::endl;

// }
