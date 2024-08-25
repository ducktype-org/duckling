#include <tester/tester.hpp>
#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/ints.hpp>


// Ref, MRef asserts:

// @note: MRef is not trivial due to "ptr = nullptr" in default constructor.
static_assert(std::is_trivial_v<Ref<int>>, "Ref should be trivially type");

static_assert(std::is_trivially_copyable_v<Ref<int>>, "Ref should be trivially copyable");
static_assert(std::is_trivially_copyable_v<MRef<int>>, "MRef should be trivially copyable");

static_assert(std::is_copy_constructible_v<Ref<int>>, "Ref should be copy constructible");
static_assert(std::is_copy_constructible_v<MRef<int>>, "MRef should be copy constructible");

static_assert(std::is_move_constructible_v<Ref<int>>, "Ref should be move constructible");
static_assert(std::is_move_constructible_v<MRef<int>>, "MRef should be move constructible");

static_assert(std::is_copy_assignable_v<Ref<int>>, "Ref should be copy assignable");
static_assert(std::is_copy_assignable_v<MRef<int>>, "MRef should be copy assignable");

static_assert(std::is_move_assignable_v<Ref<int>>, "Ref should be move assignable");
static_assert(std::is_move_assignable_v<MRef<int>>, "MRef should be move assignable");

static_assert(not std::is_constructible_v<Ref<int>, std::nullptr_t>, "Ref should not be constructible from nullptr");
static_assert(std::is_constructible_v<MRef<int>, std::nullptr_t>, "MRef should be constructible from nullptr");

// Box, MBox asserts:
static_assert(not std::is_copy_constructible_v<Box<int>>, "Box should be copy constructible");
static_assert(not std::is_copy_constructible_v<MBox<int>>, "MBox should be copy constructible");

static_assert(std::is_move_constructible_v<Box<int>>, "Box should be move constructible");
static_assert(std::is_move_constructible_v<MBox<int>>, "MBox should be move constructible");

static_assert(not std::is_copy_assignable_v<Box<int>>, "Box should not be copy assignable");
static_assert(not std::is_copy_assignable_v<MBox<int>>, "MBox should not be copy assignable");

static_assert(std::is_move_assignable_v<Box<int>>, "Box should be move assignable");
static_assert(std::is_move_assignable_v<MBox<int>>, "MBox should be move assignable");

static_assert(not std::is_constructible_v<Box<int>, std::nullptr_t>, "Box should not be constructible from nullptr");
static_assert(std::is_constructible_v<MBox<int>, std::nullptr_t>, "MBox should be constructible from nullptr");


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

struct LiveCounterInherit: public LiveCounter {};

class BoxRefTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BoxRefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("BoxRef Test") {
		TESTER_ADD_TEST(cppSanityCheck);
		TESTER_ADD_TEST(testBoxRef);
		TESTER_ADD_TEST(defaultMembersTest);
		TESTER_ADD_TEST(testMBoxMRef);
	}

private:
	void cppSanityCheck() {
		struct Ptr {
			int* ptr = nullptr;
			Ptr() = default;
			Ptr(const Ptr&) = default;
			Ptr(Ptr&&) = default;
		};

		int v = 0;
		Ptr a;
		a.ptr = &v;
		
		// NOLINTBEGIN
		int* b = std::move(a).ptr;
		int* c = std::move(a.ptr);
		// NOLINTEND
		
		ASSERT_EQUAL(a.ptr, &v);
		ASSERT_EQUAL(b, &v);
		ASSERT_EQUAL(c, &v);
	}

	void testBoxRef() {
		// basic box:
		{
			Box<LiveCounter> a = box<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Box type deduction:
		{
			Box a = box<LiveCounter>();
			auto b = box<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 2);
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

			auto a_ref_const = a_moved.ref();

			// decltype(a_ref_const->state) is just int for some reason, but "it" is still a const.
			static_assert(std::is_const_v<std::remove_pointer_t<decltype(a_ref_const.get())>>, ".ref() should return const ref");
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

			// NOLINTBEGIN
			// moving refs have no effect:
			Ref<LiveCounter> a_ref_3 = std::move(a_ref_2);
			// NOLINTEND

			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a_ref_3.get(), a_ref_2.get());
			ASSERT_EQUAL(a_ref_2.get(), a_ref_1.get());
			ASSERT_EQUAL(a_ref_1.get(), &a);
			ASSERT_TRUE(a_ref_1 == a_ref_2);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// std swap:
		{
			LiveCounter a;
			LiveCounter b;

			Ref ref_1 = &a;
			Ref ref_2 = &b;

			std::swap(ref_1, ref_2);

			ASSERT_EQUAL(ref_1, Ref(&b));
			ASSERT_EQUAL(ref_2, Ref(&a));
			ASSERT_TRUE(ref_1 != ref_2);

			Box c = box<LiveCounter>();
			Box d = box<LiveCounter>();

			Ref c_ref = c.ref();
			Ref d_ref = d.ref();

			std::swap(c, d);

			ASSERT_EQUAL(c.ref(), d_ref);
			ASSERT_EQUAL(d.ref(), c_ref);
		}

		
	}

	void defaultMembersTest() {
		struct Data {
			int data;
		};

		{
			// move + default:
			struct Container {
				MBox<Data> data_1;
				MRef<Data> data_2;

				Container() = default;
				Container(Container&&) = default;
			};

			Container c;
			Container c1 = std::move(c);
		}
		{		
			// move:
			struct Container {
				Box<Data>  data_1;
				MBox<Data> data_2;
				Ref<Data>  data_3;
				MRef<Data> data_4;

				Container(): data_1(box<Data>()), data_3(data_1.refMut()) {};
				Container(Container&&) = default;
			};

			Container c;
			Container c1 = std::move(c);
		}
		{
			// all:
			struct Container {
				Data data{0};
				Ref<Data> data_1;
				MRef<Data> data_2;

				Container(): data_1(&data) {};
				Container(const Container&) = default;
				Container(Container&&) = default;
			};

			Container a;
			Container b = a;

			// NOLINTBEGIN
			[[maybe_unused]]
			Container c = std::move(b);
			// NOLINTEND
		}



	}

	void testMBoxMRef() {
		// basic MBox:
		{
			MBox<LiveCounter> a = box<LiveCounter>();
			
			ASSERT_TRUE(a.get().has_value());
			ASSERT_TRUE(a.ref().get().has_value());
			ASSERT_TRUE(a.refMut().get().has_value());

			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// nullptr MBox:
		{
			MBox<LiveCounter> a = nullptr;

			ASSERT_TRUE(a.get().empty());
			ASSERT_TRUE(a.ref().get().empty());
			ASSERT_TRUE(a.refMut().get().empty());

			ASSERT_EQUAL(LiveCounter::count, 0);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// MBox from box:
		{
			MBox<LiveCounter> a = box<LiveCounter>();

			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// MBox type deduction:
		{
			MBox a = box<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// MXox move:
		{
			MBox<LiveCounter> a = box<LiveCounter>();
			a->state = 2;
			MBox b = std::move(a);

			ASSERT_EQUAL(b->state, 2);
			ASSERT_EQUAL(LiveCounter::count, 1);

			assertThrows<base::Panic>([&]() {
				a->state = 4;
			}, "Use after move did not throw!");
		}
		ASSERT_EQUAL(LiveCounter::count, 0);
	}
};

TESTER_COMMON_MAIN("/base/tests/");
