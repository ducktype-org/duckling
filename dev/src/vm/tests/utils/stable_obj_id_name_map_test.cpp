// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/utils/stable_obj_id_name_map.hpp>

#include <array>

class ObjIdNameMapTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ObjIdNameMapTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSimple);
		TESTER_ADD_TEST(testStable);
		TESTER_ADD_TEST(testUnstable);
	}

	base::StrID getId(u32 value) {
		static std::vector<std::string> names;
		static std::vector<base::StrID> ids;

		while (ids.size() <= value) {
			names.push_back(std::to_string(names.size()));
			ids.emplace_back(names.back().data());
		}

		return ids[value];
	}

	void testSimple() {
		std::array strs = { base::StrID("zero"), base::StrID("one"), base::StrID("two") };
		vm::ObjIdNameMap<u32> map;
		auto                  id0 = map.insert(0, strs[0]);
		ASSERT_EQUAL(0, id0);
		auto id1 = map.insert(1, strs[1]);
		ASSERT_EQUAL(1, id1);
		auto id2 = map.insert(2, strs[2]);
		ASSERT_EQUAL(2, id2);

		u64 id3 = 3;
		ASSERT_EQUAL(0, *map.at(id0));
		ASSERT_EQUAL(0, *map.at(strs[0]));
		ASSERT_EQUAL(1, *map.at(id1));
		ASSERT_EQUAL(1, *map.at(strs[1]));
		ASSERT_EQUAL(2, *map.at(id2));
		ASSERT_EQUAL(2, *map.at(strs[2]));

		ASSERT_TRUE(map.nameOf(id3).empty());

		u32 v = 0;
		for (auto& i: map) ASSERT_EQUAL(i, v++);
		for (u32 i = 0; i < map.size(); i++) ASSERT_EQUAL(i, *map.at(base::safeIntConv<usize>(i)));
		ASSERT_TRUE(map.contains(0));
		ASSERT_TRUE(map.contains(strs[0]));
		ASSERT_TRUE(!map.contains(3));
		ASSERT_TRUE(!map.contains(base::StrID("three")));
	}

	void testStable() {
		constexpr u32 OVERLOAD_SIZE = 100;

		vm::StableObjIdNameMap<u32> stable_map;
		assertTrue(0 == stable_map.insert(2 * OVERLOAD_SIZE, getId(0)), "insert to stable map(0)");
		u32* ptr = stable_map.at(0).get();
		assertTrue(*ptr == 2 * OVERLOAD_SIZE, "access through a pointer(0)");

		for (u32 i = 1; i < OVERLOAD_SIZE; ++i) {
			stable_map.insert(i, getId(i));
			std::string msg = "pointer equality(" + std::to_string(i) + ")";
			assertTrue(ptr == stable_map.at(0).get(), msg);
		}
	}

	// Testing if unstable map is actually unstable and cannot be used in place of stable map.
	void testUnstable() {
		constexpr u32 OVERLOAD_SIZE = 100;

		vm::ObjIdNameMap<u32> unstable_map;
		assertTrue(0 == unstable_map.insert(2 * OVERLOAD_SIZE, getId(0)), "insert to stable map(0)");
		u32* ptr = unstable_map.at(0).get();
		assertTrue(*ptr == 2 * OVERLOAD_SIZE, "access through a pointer(0)");

		for (u32 i = 1; i < OVERLOAD_SIZE; ++i) unstable_map.insert(i, getId(i));
		assertTrue(ptr != unstable_map.at(0).get(), "pointer inequality(0)");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/utils/");
