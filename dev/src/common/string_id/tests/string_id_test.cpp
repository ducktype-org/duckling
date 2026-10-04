#include <base/collections/maps.hpp>
#include <base/misc/raw_view.hpp>

#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

class SimpleIDMapsTest;

class A {
private:
	i32               x;
	usize             count{ 0 };
	SimpleIDMapsTest* test;

public:
	A(i32 x, SimpleIDMapsTest* test): x(x), test(test) {}

	A(const A& other);

	A(A&& other) noexcept: x(other.x), count(other.count), test(other.test) {}
};

class SimpleIDMapsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleIDMapsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicMapTest);
		TESTER_ADD_TEST(strIDTest);
		TESTER_ADD_TEST(bigStringBufferTest);
	}

	~SimpleIDMapsTest() override = default;

	friend A;

private:
	void basicMapTest() {
		base::Map<std::string, int> map;
		map.put("abc", 5);
		assertTrue(map.size() == 1, "Bad map size 1");
		assertTrue(map.notEmpty(), "Bad map size 2");
		assertTrue(map["abc"] == 5, "Bad map value 1");

		for (auto& [v, k]: map) assertTrue(map[v] == k, "Bad map value 2");

		assertTrue(map.erase("abc"), "Map element not erased");
		assertTrue(map.empty(), "Map is not empty");

		base::HashMap<int, A> map2;
		A                     a(4, this);
		map2.put(5, a);
		map2.put(3, A(3, this));

		base::VectorMap<int, int> map3;
		assertThrows<std::exception>(
			[&]() { map3[5] = 5; }, "Map should throw exception but does not"
		);

		map3.put(5, 5);
		assertTrue(map3.erase(5), "Map element not erased 2");
	}

	static base::RawView makeView(std::string_view view) {
		return {
			std::span<const std::byte>(reinterpret_cast<const std::byte*>(view.data()), view.size())
		};
	}

	void strIDTest() {
		base::StrID id1(makeView("abc"));
		base::StrID id2(makeView("abc"));
		base::StrID id3(makeView("ab"));

		assertTrue(id1 == id2, "== error");
		assertTrue(id2 != id3, "!= error");
		assertTrue(id3 == id3, "== error");
		assertTrue(id3 == "ab", "data error");
	}

	void bigStringBufferTest() {
		// A regular buffer with free space, then a string too big for any regular
		// buffer (which gets its own dedicated buffer at the end of the buffer
		// list), then a small string. The small string must open a new regular
		// buffer instead of being appended into the dedicated buffer, where it
		// would overwrite the interned bytes of the big string.
		base::StrID warmup(makeView("warmup"));

		const std::string big(40'000, 'B');
		base::StrID       big_id(makeView(big));

		base::StrID small_id(makeView("small-after-big"));

		assertTrue(big_id.view().stringView() == big, "big string corrupted by following intern");
		assertTrue(base::StrID(makeView(big)) == big_id, "big string lookup mismatch");
		assertTrue(small_id == "small-after-big", "small string data error");
	}
};

A::A(const A& other): x(other.x), count(other.count), test(other.test) {
	count++;
	test->assertTrue(count < 2, "A constructor called to many times");
}

TESTER_COMMON_MAIN("/src/base/tests/");
