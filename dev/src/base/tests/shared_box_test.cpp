#include <base/pointers/shared_box.hpp>

#include <tester/tester.hpp>

#include <thread>

// SharedBox asserts:
static_assert(std::is_copy_constructible_v<SharedBox<int>>, "SharedBox should be copy constructible");

static_assert(std::is_move_constructible_v<SharedBox<int>>, "SharedBox should be move constructible");

static_assert(std::is_copy_assignable_v<SharedBox<int>>, "SharedBox should be copy assignable");

static_assert(std::is_move_assignable_v<SharedBox<int>>, "SharedBox should be move assignable");

static_assert(
	not std::is_constructible_v<SharedBox<int>, std::nullptr_t>,
	"SharedBox should not be constructible from nullptr"
);

struct InstancesCounter {
	constinit static inline usize count = 0;

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
		TESTER_ADD_TEST(testCustomDeleter);
		TESTER_ADD_TEST(testEquality);
		TESTER_ADD_TEST(concurrentUsage<1>);
		TESTER_ADD_TEST(concurrentUsage<2>);
		TESTER_ADD_TEST(concurrentUsage<4>);
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
			auto a = new SharedBox<InstancesCounter>(makeSharedBox<InstancesCounter>());
			ASSERT_EQUAL(InstancesCounter::count, 1);
			{
				// NOLINTBEGIN
				SharedBox<InstancesCounter> b(*a);
				// NOLINTEND
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

		// Move constructor and assignment:
		{
			auto a = makeSharedBox<InstancesCounter>(1);
			{
				auto b(std::move(a));
				ASSERT_EQUAL(b->state, 1);
				ASSERT_EQUAL(InstancesCounter::count, 1);
				assertThrows<base::Panic>([&]() { *a; }, "Use after move did not throw!");
			}
			ASSERT_EQUAL(InstancesCounter::count, 0);
			auto c = makeSharedBox<InstancesCounter>(2);
			{
				auto d = makeSharedBox<InstancesCounter>(3);
				ASSERT_EQUAL(InstancesCounter::count, 2);
				d = std::move(c);
				ASSERT_EQUAL(InstancesCounter::count, 1);
			}
			ASSERT_EQUAL(InstancesCounter::count, 0);
		}

		// Ref from SharedBox:
		{
			SharedBox<InstancesCounter> a = makeSharedBox<InstancesCounter>(123);

			auto a_ref_mut = a.refMut();
			ASSERT_EQUAL(a_ref_mut->state, 123);

			a_ref_mut->state = 456;
			ASSERT_EQUAL(a->state, 456);

			// NOLINTBEGIN
			auto b(a);
			auto b_ref_const = b.ref();
			// NOLINTEND
			ASSERT_EQUAL(b->state, 456);

			a_ref_mut->state = 789;
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

	template<class T>
	struct StatefulDeleter final {
		static inline int s_state = 0;

		void del(T* ptr) {
			delete ptr;
			if (ptr != nullptr) s_state++;
		}
	};

	void testCustomDeleter() {
		{
			auto ib = SharedBox<int, StatefulDeleter<int>>::fromPointerWithCustomDeleter(
				new int(42), StatefulDeleter<int>()
			);
			ASSERT_EQUAL(*ib, 42);
			ASSERT_EQUAL(StatefulDeleter<int>::s_state, 0);
		}
		ASSERT_EQUAL(StatefulDeleter<int>::s_state, 1);
	}

	void testEquality() {
		auto a = makeSharedBox<int>(42);
		// NOLINTBEGIN
		auto b = a;
		// NOLINTEND
		auto c = makeSharedBox<int>(42);
		ASSERT_EQUAL(a == b, true);
		ASSERT_EQUAL(a == c, false);
	}

	struct DeleteCounter {
		constinit static inline usize count = 0;

		bool do_count = false;

		DeleteCounter() = default;

		~DeleteCounter() {
			if (do_count) count++;
		}
	};

	template<u64 THREAD_COUNT, u64 ITERATIONS_PER_THREAD = 1000>
	void concurrentUsage() {
		DeleteCounter::count = 0;

		SharedBox<DeleteCounter> a = makeSharedBox<DeleteCounter>();
		a->do_count                = true;

		std::vector<SharedBox<DeleteCounter>> threads_boxes;
		threads_boxes.reserve(THREAD_COUNT);
		for (u64 i = 0; i < THREAD_COUNT; i++) threads_boxes.emplace_back(a);

		assertTrue(
			threads_boxes[0].ownersCount() == THREAD_COUNT + 1,
			"All SharedBoxes should share ownership of the same object!"
		);
		a.reset();
		assertTrue(
			threads_boxes[0].ownersCount() == THREAD_COUNT,
			"All threads should share ownership of the same object!"
		);

		std::vector<std::jthread> threads;
		threads.reserve(THREAD_COUNT);
		for (u64 i = 0; i < THREAD_COUNT; i++) {
			threads.emplace_back([&, i] {
				// We perform a lot of copying and moving to increase the chances of catching
				// concurrency issues with reference counting. And after that we reset the
				// SharedBox, which should cause the DeleteCounter to be deleted exactly once.

				for (u64 j = 0; j < ITERATIONS_PER_THREAD; j++) {
					SharedBox<DeleteCounter> copy = threads_boxes[i];
					SharedBox<DeleteCounter> move = std::move(copy);

					SharedBox<DeleteCounter> copy2 = makeSharedBox<DeleteCounter>();
					copy2                          = move;

					SharedBox<DeleteCounter> move2 = makeSharedBox<DeleteCounter>();
					move2                          = std::move(copy2);

					assertTrue(
						move2 == threads_boxes[i],
						"SharedBox should still own the same object after copying and moving!"
					);
					assertTrue(
						move2.ref() == threads_boxes[i].ref(),
						"SharedBox should still own the same object after copying and moving!"
					);

					assertTrue(
						threads_boxes[i].ownersCount() >= 2,
						"At least threads_boxes[i] and move2 should share ownership of the same "
						"object!"
					);
				}
				threads_boxes[i].reset();
			});
		}

		for (auto& thread: threads) thread.join();

		for (auto& box: threads_boxes) assertTrue(!box, "All SharedBoxes should be reset!");

		ASSERT_EQUAL_PRINT(DeleteCounter::count, 1);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
