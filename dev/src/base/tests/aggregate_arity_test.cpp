#include <base/comptime/aggregate_arity.hpp>

#include <tester/tester.hpp>

#include <string>
#include <vector>

namespace {
	struct Three {
		int         a;
		double      b;
		std::string c;
	};

	struct Empty {};

	struct Base {};

	struct Derived: Base {
		int a;
	};

	struct WithArray {
		int a;
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		char tag[4];
	};
}

class AggregateArityTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS AggregateArityTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
		static_assert(base::ELIDED_ARITY_V<Three> == 3);
		static_assert(base::ELIDED_ARITY_V<Empty> == 0);
		// a C array field takes one clause per element
		static_assert(base::ELIDED_ARITY_V<WithArray> == 5);
		static_assert(base::ELIDED_ARITY_V<std::vector<int>> == base::LADDER_MAX + 1);

		static_assert(base::ACCEPTS_ANY_LENGTH_V<std::vector<int>>);
		static_assert(!base::ACCEPTS_ANY_LENGTH_V<Three>);

		static_assert(base::HAS_BASE_V<Derived>);
		static_assert(!base::HAS_BASE_V<Three>);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
