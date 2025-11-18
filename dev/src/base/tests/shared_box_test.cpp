#include <base/pointers/shared_box.hpp>
#include <tester/tester.hpp>

// SharedBox asserts:
static_assert(not std::is_copy_constructible_v<SharedBox<int>>, "SharedBox should be copy constructible");

static_assert(std::is_move_constructible_v<SharedBox<int>>, "SharedBox should be move constructible");

static_assert(std::is_copy_assignable_v<SharedBox<int>>, "SharedBox should be copy assignable");

static_assert(std::is_move_assignable_v<SharedBox<int>>, "SharedBox should be move assignable");

static_assert(
	not std::is_constructible_v<SharedBox<int>, std::nullptr_t>,
	"Box should not be constructible from nullptr"
);

class SharedBoxTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SharedBoxTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(xd);
	}

private:
    void xd() {

    }
};

TESTER_COMMON_MAIN("/src/base/tests/");