#include <tester/tester.hpp>

#include <base/maps.hpp>
#include <base/raw_view.hpp>
#include <base/string_id.hpp>

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

	static base::RawView make_view(std::string_view view) {
		return base::RawView({ reinterpret_cast<const byte*>(view.data()), view.size() });
	}

	void strIDTest() {
		base::StrID id1(make_view("abc"));
		base::StrID id2(make_view("abc"));
		base::StrID id3(make_view("ab"));

		assertTrue(id1 == id2, "== error");
		assertTrue(id2 != id3, "!= error");
		assertTrue(id3 == id3, "== error");
		assertTrue(id3 == "ab", "data error");
	}
};

A::A(const A& other): x(other.x), count(other.count), test(other.test) {
	count++;
	test->assertTrue(count < 2, "A constructor called to many times");
}

TESTER_COMMON_MAIN("/base/tests/");
