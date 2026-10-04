#include <base/collections/stable_container.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <tester/tester.hpp>

#include <ranges>

STRONG_TYPEDEF_INT_DIMENSIONAL(SomeID, usize);

class StableListTestSimple: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StableListTestSimple

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(stableHashMapTest);
		TESTER_ADD_TEST(stableHashMapMaybePutAndUpdateTest);
		TESTER_ADD_TEST(stableHashMapTestStability);
	}

private:
	void simpleTest() {
		message("Parts of this state only make sense under valgrind");

		base::StableVector<i32> vector;

		assertTrue(vector.empty(), "bad list empty");
		assertTrue(!vector.notEmpty(), "bad list not empty");
		assertTrue(vector.size() == 0, "bad list size");

		vector.pushBack(2);
		ASSERT_EQUAL(vector.lastIndex(), 0);

		vector.pushBack(3);
		vector.pushBack(4);

		ASSERT_EQUAL(vector.lastIndex(), 2);

		assertTrue(*vector[0] == 2, "Bad stable list pushBack (0)");
		assertTrue(*vector[1] == 3, "Bad stable list pushBack (1)");
		assertTrue(*vector[2] == 4, "Bad stable list pushBack (2)");

		auto ref = vector[1];

		assertTrue(*ref == 3, "Bad stable list ref");
		*ref = 4;
		assertTrue(*ref == 4, "Bad stable list ref");

		CRef<int> c_ref = vector[0];
		assertTrue(*c_ref == 2, "Bad stable list c_ref");

		for (int i = 0; i < 100; i++) vector.pushBack(i);

		// we check that it doesn't throw:
		std::ignore = vector[102];

		assertThrows<std::exception>(
			[&]() { std::ignore = vector[103]; }, "Out of range access didn't throw"
		);


		assertThrows<std::exception>(
			[&]() { std::ignore = vector[104]; }, "Out of range access didn't throw"
		);

		assertTrue(vector.size() == 103, "bad list size");
		ASSERT_EQUAL(vector.lastIndex(), 102);
		assertTrue(!vector.empty(), "bad list empty");
		assertTrue(vector.notEmpty(), "bad list not empty");

		assertTrue(*ref == 4, "Stable list ref not stable");
		assertTrue(*c_ref == 2, "Bad stable list c_ref");
	}

	template<class StableIt>
	void staticAssertIterator() {
		using ::base::StableVector;
		static_assert(std::same_as<
					  std::iter_reference_t<const StableIt>,
					  std::iter_reference_t<StableIt>>);
		static_assert(std::same_as<
					  std::iter_rvalue_reference_t<const StableIt>,
					  std::iter_rvalue_reference_t<StableIt>>);
		static_assert(std::indirectly_readable<StableIt>);
		static_assert(std::input_or_output_iterator<StableIt>);
		static_assert(std::input_iterator<StableIt>);
		static_assert(std::random_access_iterator<StableIt>);
	}

	template<class Data>
	void staticAssertRange() {
		using Vector = ::base::StableVector<Data>;

		staticAssertIterator<typename Vector::Iterator>();
		staticAssertIterator<typename Vector::ConstIterator>();
		static_assert(std::same_as<std::ranges::range_reference_t<Vector>, Data&>);
	}

	void stableVectorTestRanges() {
		using ::base::StableVector;
		staticAssertRange<const int>();
		staticAssertRange<int>();

		StableVector<int> vector;
		vector.pushBack(5);
		vector.pushBack(1);
		auto new_range = vector | std::views::transform([](int& x) { return 2 * x; })
		               | std::views::filter([](const int& x) { return x >= 5; });
		assertEqual(10, *new_range.begin(), "Transformed and filtered");
	}

	void stableHashMapTest() {
		base::StableHashMap<std::string, std::string> map;
		map.put("lol", "test");

		ASSERT_EQUAL("test", map["lol"]);
		ASSERT_EQUAL(1, map.size());

		ASSERT_EQUAL("test", map["lol"]);
		ASSERT_EQUAL(true, map.atMaybe("lol2").empty());
		ASSERT_EQUAL("test", *map.atMaybe("lol").value());

		map.clear();
		ASSERT_EQUAL(0, map.size());

		map.put("lol", "test 1");
		map.put("a", "test 2");
		map.put("b", "test 3");
		auto put_res = map.maybePut(std::string("lol"), std::string("test"));

		assertTrue(put_res == nullptr, "Value was wrongly inserted");

		ASSERT_EQUAL(3, map.size());
		ASSERT_EQUAL(map["a"], "test 2");
		ASSERT_EQUAL(map["b"], "test 3");
		ASSERT_EQUAL(map["lol"], "test 1");
	}

	void stableHashMapMaybePutAndUpdateTest() {
		base::StableHashMap<std::string, i32> map;

		auto inserted
			= map.maybePutAndUpdate(std::string("key"), 10, [](Ref<i32> value) { *value += 5; });

		assertTrue(inserted != nullptr, "Expected insertion for missing key");
		ASSERT_EQUAL(1, map.size());
		ASSERT_EQUAL(15, map["key"]);

		auto not_inserted
			= map.maybePutAndUpdate(std::string("key"), 999, [](Ref<i32> value) { *value += 2; });

		assertTrue(not_inserted == nullptr, "Expected no insertion for existing key");
		ASSERT_EQUAL(1, map.size());
		ASSERT_EQUAL(17, map["key"]);
	}

	void stableHashMapTestStability() {
		base::StableHashMap<usize, i64> map;
		const i64*                      ptr = nullptr;
		for (usize i = 0; i < 10'000; i++) {
			map.put(i, static_cast<i64>(i));
			if (i == 0) ptr = &map[0];
			assertEqual(ptr, &map[0], base::strConcat("A StableHashMap is not stable :O"));
		}
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
