#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent_array.hpp>
#include <vm/utils/persistent_hashmap.hpp>
#include <vm/utils/persistent_vector.hpp>

#include <string>
#include <utility>
#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"

class PersistentStlTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PersistentStlTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testBijective);
		TESTER_ADD_TEST(testVector);
		TESTER_ADD_TEST(testArray);
		TESTER_ADD_TEST(testHashMap);
	}

	void testBijective() {
		vm::persistent::detail::BijectiveMap<base::StrID, usize> dir{};

		base::StrID vals[10];
		for (usize i = 0; i < 10; i++) {
			vals[i] = base::StrID("val" + std::to_string(i));
			dir.emplaceByLeft(vals[i], i);
		}

		for (usize i = 0; i < 10; i++) {
			auto& rght = dir.atLeft(vals[i]);
			auto& left = dir.atRight(i);


			ASSERT_EQUAL(i, rght);
			ASSERT_EQUAL(vals[i], left);

			auto maybe_rght = dir.atLeftOpt(vals[i]);
			auto maybe_left = dir.atRightOpt(i);

			ASSERT_TRUE(maybe_left.has_value());
			ASSERT_TRUE(maybe_rght.has_value());

			ASSERT_EQUAL(i, *maybe_rght);
			ASSERT_EQUAL(vals[i], *maybe_left);

			auto [inserted, refR] = dir.emplaceByLeft(vals[i], 11);
			ASSERT_TRUE(!inserted);
		}

		base::StrID new_val("new_val");
		auto [inserted, refR] = dir.emplaceByLeft(new_val, 11);

		ASSERT_TRUE(inserted);
	}

	void testVector() {
		using namespace vm::persistent;

		Vector<std::string> vec;
		auto checker = [&](VectorStateID state, std::vector<std::string> expected) -> void {
			auto size = expected.size();
			ASSERT_EQUAL(expected.size(), vec.size(state));
            auto view = vec.view(state, 0, size);
            ASSERT_EQUAL(expected, view);

            for (usize i = 1; i <= size; i++) {
	            ASSERT_EQUAL(expected[i - 1], vec.access(state, i));            
			}
		};

		auto                                empt = vec.getEmpty();
		checker(empt, {});

		auto op01 = vec.push(empt, std::string("val01"));
		checker(op01, { "val01" });

		auto op02 = vec.push(op01, std::string("val02"));
		checker(op02, { "val01", "val02" });

		auto op03 = vec.push(op02, std::string("val03"));
		checker(op03, { "val01", "val02", "val03" });

		auto op04 = vec.getPrefix(op03, 2);
		checker(op04, { "val01", "val02" });

		auto op05 = vec.pop(op03);
		checker(op05, { "val01", "val02" });

		auto op06 = vec.push(op03, std::string("val05"));
		checker(op06, { "val01", "val02", "val03", "val05" });

		auto op07 = vec.push(op06, std::string("val06"));
		checker(op07, { "val01", "val02", "val03", "val05", "val06" });

		auto op08 = vec.change(op07, 3, std::string("val07"));
		checker(op08, { "val01", "val02", "val07", "val05", "val06" });

		auto op09 = vec.change(op08, 2, std::string("val08"));
		checker(op09, { "val01", "val08", "val07", "val05", "val06" });

		auto op10 = vec.push(op09, std::string("val09"));
		checker(op10, { "val01", "val08", "val07", "val05", "val06" , "val09"});
			
		auto op11 = vec.pop(op10, 2);
		checker(op11, { "val01", "val08", "val07", "val05"});

		auto op12 = vec.change(op11, 1, std::string("val11"));
		checker(op12, { "val11", "val08", "val07", "val05"});

		auto op13 = vec.change(op12, 2, std::string("val02"));
		checker(op13, { "val11", "val02", "val07", "val05"});

		auto op14 = vec.change(op13, 1, std::string("val01"));
		checker(op14, { "val01", "val02", "val07", "val05"});

		auto op15 = vec.change(op14, 3, std::string("val03"));
		checker(op15, { "val01", "val02", "val03", "val05"});

		auto op16 = vec.getPrefix(op15, 3);
		checker(op16, { "val01", "val02", "val03" });

		ASSERT_EQUAL(op16, op03);
		ASSERT_EQUAL(op02, op04);
		ASSERT_EQUAL(op05, op04);
		
	}

	void testArray() {
		using namespace vm::persistent;

		using act_t = std::map<usize, std::string>;

		Array<std::string> array{ 5 };

		auto checker = [&](ArrayStateID state, act_t expected) -> void {
			static constexpr usize SIZE = (1 << 5);

			for (usize idx = 1; idx <= SIZE; idx++) {
				if (expected.contains(idx)) {
					ASSERT_EQUAL(expected[idx], array.access(state, idx));            
				}
				else {
					ASSERT_TRUE(!array.active(state, idx));
				}
			}
		};

		auto empty = array.getEmpty();
		checker(empty, {});

		auto op01 = array.change(empty, 1, "val01");
		checker(op01, { { 1, "val01" } });

		auto op02 = array.change(op01, 2, "val02");
		checker(op02, { { 1, "val01" },{ 2, "val02" } });

		auto op03 = array.change(op02, 1, "val03");
		checker(op03, { { 1, "val03" }, { 2, "val02" } });

		auto op04 = array.change(op01, 1, "val04");
		checker(op04, { { 1, "val04" } });

		auto op05 = array.change(op04, 4, "val05");
		checker(op05, { { 1, "val04" }, { 4, "val05" } });

		auto op06 = array.change(op05, 27, "val06");
		checker(op06, { { 1, "val04" }, { 4, "val05" }, {27, "val06"} });
		
		auto op07 = array.change(op06, 31, "val07");
		checker(op07, { { 1, "val04" }, { 4, "val05" }, { 27, "val06" }, { 31, "val07" } });

		auto op08 = array.erase(op07, 27);
		checker(op08, { { 1, "val04" }, { 4, "val05" }, { 31, "val07" } });

		auto op09 = array.erase(op08, 1);
		checker(op09, { { 4, "val05" }, { 31, "val07" } });

		auto op10 = array.erase(op09, 31);
		checker(op10, { { 4, "val05" } });

		auto op11 = array.erase(op10, 4);
		checker(op11, { {} });

		ASSERT_EQUAL(empty, op11);
	}

	void testHashMap() {}
};

TESTER_COMMON_MAIN("/src/vm/tests/utils/");
