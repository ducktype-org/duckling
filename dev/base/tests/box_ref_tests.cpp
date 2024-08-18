#include <tester/tester.hpp>
#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/ints.hpp>


struct LiveCounter {
	static inline usize count = 0;
	
	LiveCounter() {
		count++;
	}

	LiveCounter(const LiveCounter&) {
		count++;
	}

	LiveCounter(LiveCounter&&) noexcept {
		count++;
	}

	~LiveCounter() {
		count--;
	}
};


class BoxRefTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BoxRefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("BoxRef Test") {
		TESTER_ADD_TEST(testBoxRef);
	}

private:
	void testBoxRef() {
		{
			Box<LiveCounter> a = box<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);
	}
};

TESTER_COMMON_MAIN("/base/tests/");
