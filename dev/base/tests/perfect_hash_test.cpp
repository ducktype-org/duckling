#include <tester/tester.hpp>
#include <base/perfect_hash.hpp>

struct TypeWithHash {
	u64 a;

	[[nodiscard]]
	base::HashT customPerfectHash() const {
		return a;
	}
};

struct TypeWithoutHash {
	u64 a;
};

base::HashT customPerfectHash(const TypeWithoutHash& key) { return key.a; }

class PerfectHashTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PerfectHashTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testPerfectHash); }

private:
	void testPerfectHash() {
		constexpr TypeWithHash    to_hash_1{ 1 };
		constexpr TypeWithoutHash to_hash_2{ 2 };

		ASSERT_EQUAL(base::perfectHash(to_hash_1), 1);
		ASSERT_EQUAL(base::perfectHash(to_hash_2), 2);
		ASSERT_EQUAL(base::perfectHash(123), 123);
	}
};

TESTER_COMMON_MAIN("/base/tests/");
