#include <tester/tester.hpp>
#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/ints.hpp>


struct LiveCounter {
	static inline usize count = 0;

	int state = 0;
	
	LiveCounter() {
		count++;
	}

	LiveCounter(int state): state(state) {
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

struct LiveCounterInherit: public LiveCounter{

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
		// basic box:
		{
			Box<LiveCounter> a = box<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// box move:
		{
			Box<LiveCounter> a = box<LiveCounter>();
			a->state = 2;
			Box<LiveCounter> b = std::move(a);

			ASSERT_EQUAL(b->state, 2);
			ASSERT_EQUAL(LiveCounter::count, 1);

			assertThrows<base::Panic>([&]() {
				a->state = 4;
			}, "Use after move did not throw!");
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Ref from box:
		{
			Box<LiveCounter> a = box<LiveCounter>(123);
			ASSERT_EQUAL(a->state, 123);

			auto a_ref = a.refMut();
			ASSERT_EQUAL(a_ref->state, 123);

			a_ref->state = 456;
			ASSERT_EQUAL(a->state, 456);

			auto a_moved = std::move(a);
			ASSERT_EQUAL(a_ref->state, 456);

			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Ref template deduction:
		{
			LiveCounter a;

			Ref a_ref_1 = &a;
			Ref<LiveCounter> a_ref_2 = &a;

			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a_ref_1.get(), a_ref_2.get());
			ASSERT_EQUAL(a_ref_1.get(), &a);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Construction from inheriting class:
		{
			LiveCounterInherit a;

			[[maybe_unused]]
			Ref<LiveCounter> a_ref = &a;
			ASSERT_EQUAL(LiveCounter::count, 1);
			
			Box<LiveCounter> live = box<LiveCounterInherit>();
			ASSERT_EQUAL(LiveCounter::count, 2);

		}
		ASSERT_EQUAL(LiveCounter::count, 0);
		
		// Copy refs:
		{
			LiveCounter a;
			Ref<LiveCounter> a_ref_1 = &a;
			Ref<LiveCounter> a_ref_2 = a_ref_1;

			// moving refs have no effect:
			Ref<LiveCounter> a_ref_3 = std::move(a_ref_2);

			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a_ref_3.get(), a_ref_2.get());
			ASSERT_EQUAL(a_ref_2.get(), a_ref_1.get());
			ASSERT_EQUAL(a_ref_1.get(), &a);
			ASSERT_TRUE(a_ref_1 == a_ref_2);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// std swap ref:
		{
			LiveCounter a;
			LiveCounter b;

			Ref ref_1 = &a;
			Ref ref_2 = &b;

			std::swap(ref_1, ref_2);

			ASSERT_EQUAL(ref_1, Ref(&b));
			ASSERT_EQUAL(ref_2, Ref(&a));
			ASSERT_TRUE(ref_1 != ref_2);
		}
	}
};

TESTER_COMMON_MAIN("/base/tests/");
