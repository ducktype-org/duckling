#include <tester/tester.hpp>

// placeholder for now

class HirTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HirTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("HIR test") {}

private:
};

TESTER_COMMON_MAIN("/RiftCompiler/src/hir/tests/");
