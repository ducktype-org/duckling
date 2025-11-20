#include <base/pointers/shared_box.hpp>
#include <tester/tester.hpp>

// SharedBox asserts:
static_assert(std::is_copy_constructible_v<SharedBox<int>>, "SharedBox should be copy constructible");

static_assert(std::is_move_constructible_v<SharedBox<int>>, "SharedBox should be move constructible");

static_assert(std::is_copy_assignable_v<SharedBox<int>>, "SharedBox should be copy assignable");

static_assert(std::is_move_assignable_v<SharedBox<int>>, "SharedBox should be move assignable");

static_assert(
	not std::is_constructible_v<SharedBox<int>, std::nullptr_t>,
	"Box should not be constructible from nullptr"
);

struct LiveCounter {
	static inline usize count = 0;

	int state = 0;

	LiveCounter() { count++; }

	LiveCounter(int state): state(state) { count++; }

	LiveCounter(const LiveCounter&) { count++; }

	LiveCounter(LiveCounter&&) noexcept { count++; }

	~LiveCounter() { count--; }
};

class SharedBoxTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SharedBoxTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSharedBox);
	}

private:
    void testSharedBox() {
		// basic SharedBox
		{
			SharedBox<LiveCounter> a = makeSharedBox<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// SharedBox copy constructor and owners counter
		{
			SharedBox<LiveCounter>* a = new SharedBox<LiveCounter>(makeSharedBox<LiveCounter>());
			ASSERT_EQUAL(LiveCounter::count, 1);
			{
				SharedBox<LiveCounter> b(*a);
				ASSERT_EQUAL(LiveCounter::count, 1);
				delete a;
				ASSERT_EQUAL(LiveCounter::count, 1);
			}
			ASSERT_EQUAL(LiveCounter::count, 0);
		}
    }
};

TESTER_COMMON_MAIN("/src/base/tests/");