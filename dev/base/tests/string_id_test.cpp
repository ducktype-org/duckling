#include <tester/tester.hpp>
#include <lexer/lexer.hpp>
#include <filesystem/file.hpp>
#include <base/raw_view.hpp>
#include <base/maps.hpp>

class SimpleIdMapsTest;

class A {
private:
	int x;
	size_t count;
	SimpleIdMapsTest& test;
public:
	A(int x, SimpleIdMapsTest& test) : x(x), count(0), test(test) {}

	A(const A& other);

	A(A&& other) noexcept: x(other.x), count(other.count), test(other.test) {}
};


class SimpleIdMapsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleIdMapsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple FileSystem Test") {
		TESTER_ADD_TEST(basicMapTest);
		TESTER_ADD_TEST(simpleIdTest);
		TESTER_ADD_TEST(strIdTest);
	}

	~SimpleIdMapsTest() override = default;

	friend A;
private:

	void basicMapTest() {
		base::Map<std::string, int> map;
		map.put("abc", 5);
		assert(map.size() == 1, "Bad map size 1");
		assert(map.notEmpty(), "Bad map size 2");
		assert(map["abc"] == 5, "Bad map value 1");

		for(auto& [v, k]: map) {
			assert(map[v] == k, "Bad map value 2");
		}

		assert(map.erase("abc"), "Map element not erased");
		assert(map.empty(), "Map is not empty");

		base::HashMap<int, A> map2;
		A a(4, *this);
		map2.put(5, a);
		map2.put(3, A(3, *this));

		base::VectorMap<int, int> map3;
		assertThrows<std::exception>(
			[&](){ map3[5] = 5; },
			"Map should throw exception but does not"
		);
		
		map3.put(5, 5);
		assert(map3.erase(5), "Map element not erased 2");
	}

	struct MyIdName {};
	typedef base::NamedId<MyIdName> MyId;

	typedef base::NamedId<base::Number<123> > MyId2;
	typedef base::NamedId<base::Number<124> > MyId3;

	void simpleIdTest() {
		MyId id_1 = MyId::next();
		MyId id_2 = MyId::next();
		MyId id_3;
		assert(id_1 != id_2, "!= error");
		assert(id_1 == id_1, "== error");
		assert(!(id_1 == id_2), "== error");
		assert(id_1 < id_2, "< error");
		assert(id_3.isBad(), "isBad error");
		assert(!(id_1 > id_2), "> error");
		assert(id_1.range() == 2, "range error");
	}

	static base::RawView make_view(std::string_view view) {
		return base::RawView({reinterpret_cast<const uint8_t*>(view.data()), view.size()});
	}

	void strIdTest() {
		base::StrId id1(make_view("abc"));
		base::StrId id2(make_view("abc"));
		base::StrId id3(make_view("ab"));

		assert(id1 == id2, "== error");
		assert(id2 != id3, "!= error");
		assert(id3 == id3, "== error");
		assert(id3 == "ab", "data error");
	}

};

A::A(const A& other): x(other.x), count(other.count), test(other.test) {
	count++;
	test.assert(count < 2, "A constructor called to many times");
}

TESTER_COMMON_MAIN("/base/tests/");
