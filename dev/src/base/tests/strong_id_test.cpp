// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <tester/tester.hpp>


STRONG_TYPEDEF_ID(A);
STRONG_TYPEDEF_ID(B);

STRONG_TYPEDEF_ID_DIRECT_CREATION(Direct);

ID_STD_HASH(A);
ID_STD_HASH(Direct);

template<class T>
auto hash(const T& t) {
	return std::hash<T>{}(t);
}

class StrongIDTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StrongIDTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicTest);
		TESTER_ADD_TEST(directTest);
	}

	void basicTest() {
		auto id0 = A::next();
		ASSERT_EQUAL(id0.asInt(), 0);
		ASSERT_EQUAL(u64(id0), 0);

		auto id1 = A::next();
		ASSERT_EQUAL(id1.asInt(), 1);
		ASSERT_EQUAL(u64(id1), 1);
		assertTrue(id1.isGood(), "ID is not good.");

		std::ignore = B::next();
		std::ignore = B::next();
		std::ignore = B::next();

		ASSERT_EQUAL(id0.asInt(), 0);
		ASSERT_EQUAL(u64(id0), 0);
		ASSERT_EQUAL(id1.asInt(), 1);
		ASSERT_EQUAL(u64(id1), 1);

		auto id_copy_1_a = id1;
		auto id_copy_1_b = id1;

		ASSERT_EQUAL(id0.asInt(), 0);
		ASSERT_EQUAL(u64(id0), 0);
		ASSERT_EQUAL(id1.asInt(), 1);
		ASSERT_EQUAL(u64(id1), 1);

		ASSERT_EQUAL(id_copy_1_a.asInt(), 1);
		ASSERT_EQUAL(id_copy_1_b.asInt(), 1);

		ASSERT_EQUAL(hash(id_copy_1_b), 1);
		ASSERT_EQUAL(hash(id0), 0);

		ASSERT_EQUAL(usize(id0), 0);

		B bad_id_b;
		A bad_id_a;
		assertTrue(bad_id_a.isBad(), "Bad BadID");
		assertTrue(bad_id_b.isBad(), "Bad BadID");
		assertTrue(id_copy_1_a.isGood(), "Bad BadID");

		assertEqual(bad_id_b, B::bad(), "Bad not equal to bad");
		assertEqual(id1, id_copy_1_a, "Good not equal to Good");
	}

	void directTest() {
		for (u64 i = 0; i < 10; i++) {
			auto id = Direct::fromU64(i);
			ASSERT_EQUAL(id.asInt(), i);
			ASSERT_EQUAL(u64(id), i);
			ASSERT_EQUAL(id, Direct(i));
			ASSERT_TRUE(id.isGood());
			ASSERT_TRUE(not id.isBad());

			ASSERT_TRUE(id <= id);
			ASSERT_TRUE(id >= id);
			ASSERT_TRUE(id == id);
			ASSERT_TRUE(not(id != id));
			ASSERT_TRUE(not(id < id));
			ASSERT_TRUE(not(id > id));
			ASSERT_TRUE(id < Direct::fromU64(i + 1));
			ASSERT_TRUE(id < Direct::bad());
		}

		Direct bad_id_1;
		Direct bad_id_2 = Direct::bad();

		ASSERT_EQUAL(bad_id_1, bad_id_2);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
