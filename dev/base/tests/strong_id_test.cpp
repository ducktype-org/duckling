#include <tester/tester.hpp>
#include <base/strongly_typed_id.hpp>
#include <string>

STRONG_TYPEDEF_ID(A);
STRONG_TYPEDEF_ID(B);

ID_STD_HASH(A);

template<class T>
auto hash(const T& t) {
	return std::hash<T>{}(t);
}

class StrongIdTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StrongIdTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Strong ID test") { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		auto id0 = A::next();
		ASSERT_EQUAL(id0.asInt(), 0);
		ASSERT_EQUAL(u64(id0), 0);
	
		auto id1 = A::next();
		ASSERT_EQUAL(id1.asInt(), 1);
		ASSERT_EQUAL(u64(id1), 1);
		assert(id1.isGood(), "Id is not good.");
	
		(void)B::next();
		(void)B::next();
		(void)B::next();

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
		assert(bad_id_a.isBad(), "Bad BadID");
		assert(bad_id_b.isBad(), "Bad BadID");
		assert(id_copy_1_a.isGood(), "Bad BadID");

		assertEqual(bad_id_b, B::bad(), "Bad not equal to bad");
		assertEqual(id1, id_copy_1_a, "Good not equal to Good");
	}
};

TESTER_COMMON_MAIN("/base/tests/");
