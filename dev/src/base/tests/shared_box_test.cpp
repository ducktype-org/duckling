#include <base/pointers/shared_box.hpp>
#include <tester/tester.hpp>

// SharedBox asserts:
static_assert(std::is_copy_constructible_v<SharedBox<int>>, "SharedBox should be copy constructible");

static_assert(not std::is_move_constructible_v<SharedBox<int>>, "SharedBox should not be move constructible");

static_assert(std::is_copy_assignable_v<SharedBox<int>>, "SharedBox should be copy assignable");

static_assert(not std::is_move_assignable_v<SharedBox<int>>, "SharedBox should not be move assignable");

static_assert(
	not std::is_constructible_v<SharedBox<int>, std::nullptr_t>,
	"Box should not be constructible from nullptr"
);

struct InstancesCounter {
	static inline usize count = 0;

	int state = 0;

	InstancesCounter() { count++; }

	InstancesCounter(int state): state(state) { count++; }

	InstancesCounter(const InstancesCounter&) { count++; }

	InstancesCounter(InstancesCounter&&) noexcept { count++; }

	~InstancesCounter() { count--; }
};

class SharedBoxTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SharedBoxTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSharedBox);
		TESTER_ADD_TEST(testSharedBoxFromPtr);
	}

private:
    void testSharedBox() {
		// basic SharedBox:
		{
			SharedBox<InstancesCounter> a = makeSharedBox<InstancesCounter>();
			ASSERT_EQUAL(InstancesCounter::count, 1);
		}
		ASSERT_EQUAL(InstancesCounter::count, 0);

		// SharedBox owners counter (and copy constructor):
		{
			SharedBox<InstancesCounter>* a = new SharedBox<InstancesCounter>(makeSharedBox<InstancesCounter>());
			ASSERT_EQUAL(InstancesCounter::count, 1);
			{
				SharedBox<InstancesCounter> b(*a);
				ASSERT_EQUAL(InstancesCounter::count, 1);
				delete a;
				ASSERT_EQUAL(InstancesCounter::count, 1);
			}
			ASSERT_EQUAL(InstancesCounter::count, 0);
		}

		// Multiple SharedBoxes:
		{
			auto a = makeSharedBox<InstancesCounter>();
			{
				auto b = makeSharedBox<InstancesCounter>();
				ASSERT_EQUAL(InstancesCounter::count, 2);
			}
			ASSERT_EQUAL(InstancesCounter::count, 1);
		}
		ASSERT_EQUAL(InstancesCounter::count, 0);

		// * and ->:
		{
			auto a = makeSharedBox<InstancesCounter>(1);
			ASSERT_EQUAL((*a).state, 1);
			(*a).state = 2;
			ASSERT_EQUAL(a->state, 2);
			a->state = 3;
			ASSERT_EQUAL(a->state, 3);
		}

		// Copy constructor and assignment:
		{
			auto a = makeSharedBox<InstancesCounter>(4);
			auto c(a);
			ASSERT_EQUAL(c->state, 4);
			{
				auto b = makeSharedBox<InstancesCounter>(5);
				ASSERT_EQUAL(InstancesCounter::count, 2);
				a = b;
				ASSERT_EQUAL(a->state, 5);
			}
			ASSERT_EQUAL(InstancesCounter::count, 2);
		}
		ASSERT_EQUAL(InstancesCounter::count, 0);

		// Ref from SharedBox:
		{
			SharedBox<InstancesCounter> a = makeSharedBox<InstancesCounter>(123);

			auto a_ref = a.refMut();
			ASSERT_EQUAL(a_ref->state, 123);

			a_ref->state = 456;
			ASSERT_EQUAL(a->state, 456);

			auto b(a);
			auto b_ref_const = b.ref();
			ASSERT_EQUAL(b->state, 456);

			a_ref->state = 789;
			ASSERT_EQUAL(b->state, 789);

			// decltype(b_ref_const->state) is just int for some reason, but "it" is still a const.
			static_assert(
				std::is_const_v<std::remove_pointer_t<decltype(b_ref_const.get())>>,
				".ref() should return const ref"
			);
		}
		ASSERT_EQUAL(InstancesCounter::count, 0);

		// std swap:
		{
			SharedBox a = makeSharedBox<InstancesCounter>();
			SharedBox b = makeSharedBox<InstancesCounter>();

			Ref a_ref = a.ref();
			Ref b_ref = b.ref();

			std::swap(a, b);

			ASSERT_EQUAL(a.ref(), b_ref);
			ASSERT_EQUAL(b.ref(), a_ref);
		}
    }

	void testSharedBoxFromPtr() {
		int* ptr = new int(42);
		ASSERT_EQUAL(42, *ptr);
		auto b = SharedBox<int>::fromPointer(ptr);
		ASSERT_EQUAL(42, *b);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");