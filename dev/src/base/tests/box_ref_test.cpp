#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>


// Ref, MRef asserts:

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

static_assert(
	not std::is_constructible_v<Ref<int>, std::nullptr_t>,
	"Ref should not be constructible from nullptr"
);
static_assert(
	std::is_constructible_v<MRef<int>, std::nullptr_t>, "MRef should be constructible from nullptr"
);

// Box, MBox asserts:
static_assert(not std::is_copy_constructible_v<Box<int>>, "Box should be copy constructible");
static_assert(not std::is_copy_constructible_v<MBox<int>>, "MBox should be copy constructible");

static_assert(std::is_move_constructible_v<Box<int>>, "Box should be move constructible");
static_assert(std::is_move_constructible_v<MBox<int>>, "MBox should be move constructible");

static_assert(not std::is_copy_assignable_v<Box<int>>, "Box should not be copy assignable");
static_assert(not std::is_copy_assignable_v<MBox<int>>, "MBox should not be copy assignable");

static_assert(std::is_move_assignable_v<Box<int>>, "Box should be move assignable");
static_assert(std::is_move_assignable_v<MBox<int>>, "MBox should be move assignable");

static_assert(
	not std::is_constructible_v<Box<int>, std::nullptr_t>,
	"Box should not be constructible from nullptr"
);
static_assert(
	std::is_constructible_v<MBox<int>, std::nullptr_t>, "MBox should be constructible from nullptr"
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

struct LiveCounterInherit: public LiveCounter {};

class BoxRefTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BoxRefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(cppSanityCheck);
		TESTER_ADD_TEST(testBoxRef);
		TESTER_ADD_TEST(testBoxFromPtr);
		TESTER_ADD_TEST(defaultMembersTest);
		TESTER_ADD_TEST(testMBoxMRef);
		TESTER_ADD_TEST(testDeleters);
	}

private:
	void cppSanityCheck() {
		struct Ptr {
			int* ptr        = nullptr;
			Ptr()           = default;
			Ptr(const Ptr&) = default;
			Ptr(Ptr&&)      = default;
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
		// basic Box:
		{
			Box<LiveCounter> a = makeBox<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Box type deduction:
		{
			Box  a = makeBox<LiveCounter>();
			auto b = makeBox<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 2);

			Box c = makeBox<const LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 3);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Box move:
		{
			Box<LiveCounter> a = makeBox<LiveCounter>();
			a->state           = 2;
			Box<LiveCounter> b = std::move(a);

			ASSERT_EQUAL(b->state, 2);
			ASSERT_EQUAL(LiveCounter::count, 1);

			(*b).state = 4;
			ASSERT_EQUAL(b->state, 4);

			// NOLINTBEGIN
			// here linter detected use after move:
			assertThrows<base::Panic>([&]() { a->state = 4; }, "Use after move did not throw!");
			// NOLINTEND

			assertThrows<base::Panic>([&]() { *a; }, "Use after move did not throw!");
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Ref from Box:
		{
			Box<LiveCounter> a = makeBox<LiveCounter>(123);
			ASSERT_EQUAL(a->state, 123);

			auto a_ref = a.refMut();
			ASSERT_EQUAL(a_ref->state, 123);

			a_ref->state = 456;
			ASSERT_EQUAL(a->state, 456);

			auto a_moved = std::move(a);
			ASSERT_EQUAL(a_ref->state, 456);

			ASSERT_EQUAL(LiveCounter::count, 1);

			[[maybe_unused]] auto a_ref_const = a_moved.ref();

			// decltype(a_ref_const->state) is just int for some reason, but "it" is still a const.
			static_assert(
				std::is_const_v<std::remove_pointer_t<decltype(a_ref_const.get())>>,
				".ref() should return const ref"
			);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Ref template deduction:
		{
			LiveCounter a;

			Ref              a_ref_1 = &a;
			Ref<LiveCounter> a_ref_2 = &a;

			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a_ref_1.get(), a_ref_2.get());
			ASSERT_EQUAL(a_ref_1.get(), &a);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Construction from inheriting class:
		{
			LiveCounterInherit a;

			[[maybe_unused]] Ref<LiveCounter> a_ref = &a;
			ASSERT_EQUAL(LiveCounter::count, 1);

			Box<LiveCounter> live = makeBox<LiveCounterInherit>();
			ASSERT_EQUAL(LiveCounter::count, 2);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Copy refs:
		{
			LiveCounter      a;
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

			Box c = makeBox<LiveCounter>();
			Box d = makeBox<LiveCounter>();

			Ref c_ref = c.ref();
			Ref d_ref = d.ref();

			std::swap(c, d);

			ASSERT_EQUAL(c.ref(), d_ref);
			ASSERT_EQUAL(d.ref(), c_ref);
		}
	}

	void testBoxFromPtr() {
		auto a = makeBox<int>(7);
		ASSERT_EQUAL(7, *a);

		int* ptr = new int(42);
		ASSERT_EQUAL(42, *ptr);
		auto b = Box<int>::fromPointer(ptr);
		ASSERT_EQUAL(42, *b);
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

				Container()            = default;
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

				Container(): data_1(makeBox<Data>()), data_3(data_1.refMut()) {}

				Container(Container&&) = default;
			};

			Container c;
			Container c1 = std::move(c);
		}
		{
			// all:
			struct Container {
				Data       data{ 0 };
				Ref<Data>  data_1;
				MRef<Data> data_2;

				Container(): data_1(&data) {}

				Container(const Container&) = default;
				Container(Container&&)      = default;
			};

			Container a;
			Container b = a;

			// NOLINTBEGIN
			[[maybe_unused]] Container c = std::move(b);
			// NOLINTEND
		}
	}

	void testMBoxMRef() {
		// basic MBox:
		{
			MBox<LiveCounter> a = makeBox<LiveCounter>();

			ASSERT_TRUE(a.toOpt().has_value());
			ASSERT_TRUE(a.ref().toOpt().has_value());
			ASSERT_TRUE(a.refMut().toOpt().has_value());

			ASSERT_EQUAL(LiveCounter::count, 1);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// nullptr MBox:
		{
			MBox<LiveCounter> a = nullptr;

			ASSERT_TRUE(a.toOpt().empty());
			ASSERT_TRUE(a.ref().toOpt().empty());
			ASSERT_TRUE(a.refMut().toOpt().empty());

			ASSERT_EQUAL(LiveCounter::count, 0);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// MBox type deduction:
		{
			MBox a = makeBox<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);

			MBox b = makeBox<const LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 2);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// MBox move:
		{
			MBox<LiveCounter> a = makeBox<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);

			a->state = 2;
			MBox b   = std::move(a);

			ASSERT_EQUAL(b->state, 2);
			ASSERT_EQUAL(LiveCounter::count, 1);

			(*b).state = 4;
			ASSERT_EQUAL(b->state, 4);

			// NOLINTBEGIN
			// here linter detected use after move:
			assertThrows<base::Panic>([&]() { a->state = 4; }, "Use after move did not throw!");
			// NOLINTEND

			assertThrows<base::Panic>([&]() { *a; }, "Use after move did not throw!");
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Ref from Box:
		{
			MBox<LiveCounter> a = makeBox<LiveCounter>(123);
			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a->state, 123);

			auto a_ref = a.refMut();
			ASSERT_EQUAL(a_ref->state, 123);

			a_ref.toOpt().value()->state = 456;
			ASSERT_EQUAL(a->state, 456);
			ASSERT_EQUAL(a_ref->state, 456);

			auto a_moved = std::move(a);
			ASSERT_EQUAL(a_ref->state, 456);

			ASSERT_EQUAL(LiveCounter::count, 1);

			auto                  a_ref_const = a_moved.ref();
			[[maybe_unused]] auto pointer     = a_ref_const.toOpt().value().get();

			// decltype(a_ref_const->state) is just int for some reason, but "it" is still a const.
			static_assert(
				std::is_const_v<std::remove_pointer_t<decltype(pointer)>>,
				".ref() should return const ref"
			);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// MRef template deduction:
		{
			LiveCounter a;

			MRef              a_ref_1 = &a;
			MRef<LiveCounter> a_ref_2 = &a;

			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a_ref_1.toOpt(), a_ref_2.toOpt());
			ASSERT_EQUAL(a_ref_1.toOpt(), Ref(&a));

			const LiveCounter b;

			MRef                    b_ref_1 = &b;
			MRef<const LiveCounter> b_ref_2 = &b;

			ASSERT_EQUAL(LiveCounter::count, 2);
			ASSERT_EQUAL(b_ref_1.toOpt(), b_ref_2.toOpt());
			ASSERT_EQUAL(b_ref_1.toOpt(), Ref(&b));
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Construction from inheriting class:
		{
			LiveCounterInherit a;

			[[maybe_unused]] MRef<LiveCounter> a_ref = &a;
			ASSERT_EQUAL(LiveCounter::count, 1);

			MBox<LiveCounter> live = makeBox<LiveCounterInherit>();
			ASSERT_EQUAL(LiveCounter::count, 2);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// Copy MRefs:
		{
			LiveCounter       a;
			MRef<LiveCounter> a_ref_1 = &a;
			MRef<LiveCounter> a_ref_2 = a_ref_1;

			// NOLINTBEGIN
			// moving refs have no effect:
			MRef<LiveCounter> a_ref_3 = std::move(a_ref_2);
			// NOLINTEND

			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_EQUAL(a_ref_3.toOpt(), a_ref_2.toOpt());
			ASSERT_EQUAL(a_ref_2.toOpt(), a_ref_1.toOpt());
			ASSERT_EQUAL(a_ref_1.toOpt(), Ref(&a));
			ASSERT_TRUE(a_ref_1 == a_ref_2);

			MRef<LiveCounter> a_ref_4 = a_ref_1.toOpt().value().toMRef();
			ASSERT_TRUE(a_ref_1 == a_ref_4);
		}
		ASSERT_EQUAL(LiveCounter::count, 0);

		// std swap:
		{
			LiveCounter a;
			LiveCounter b;

			MRef ref_1 = &a;
			MRef ref_2 = &b;

			std::swap(ref_1, ref_2);

			ASSERT_EQUAL(ref_1, MRef(&b));
			ASSERT_EQUAL(ref_2, MRef(&a));
			ASSERT_TRUE(ref_1 != ref_2);

			Box c = makeBox<LiveCounter>();
			Box d = makeBox<LiveCounter>();

			MRef c_ref = c.ref();
			MRef d_ref = d.ref();

			std::swap(c, d);

			ASSERT_EQUAL(c.ref(), d_ref);
			ASSERT_EQUAL(d.ref(), c_ref);
		}

		// move MBox into Box:
		{
			MBox a = makeBox<LiveCounter>();
			ASSERT_EQUAL(LiveCounter::count, 1);

			Box b = std::move(a).toOptBox().value();
			ASSERT_EQUAL(LiveCounter::count, 1);
			ASSERT_TRUE(std::move(a).toOpt().empty());

			// @TODO: assert that this does not compile:
			// Box c = a.stealBox();

			assertThrows<base::Panic>([&]() { *a; }, "Use after move did not throw!");
		}
		ASSERT_EQUAL(LiveCounter::count, 0);
	}

	template<class T>
	struct StatefulDeleter final {
		int               state   = 0;
		static inline int s_state = 0;

		void del(T* ptr) {
			delete ptr;
			if (ptr != nullptr) {
				state++;
				s_state++;
			}
		}
	};

	template<class T>
	struct FromStatefulDeleterByValue final {
		FromStatefulDeleterByValue() = default;

		FromStatefulDeleterByValue(StatefulDeleter<T>) {}

		void del(T* ptr) { delete ptr; }
	};

	template<class T>
	struct FromStatefulDeleterByCopy final {
		FromStatefulDeleterByCopy() = default;

		FromStatefulDeleterByCopy(const StatefulDeleter<T>&) {}

		void del(T* ptr) { delete ptr; }
	};

	template<class T>
	struct FromStatefulDeleterByMove final {
		FromStatefulDeleterByMove() = default;

		FromStatefulDeleterByMove(StatefulDeleter<T>&& a) { (void) std::move(a); }

		void del(T* ptr) { delete ptr; }
	};

	void testDeleters() {
		{
			auto ib = Box<int, StatefulDeleter<int>>::fromPointerWithCustomDeleter(
				new int(42), StatefulDeleter<int>{ 7 }
			);
			ASSERT_EQUAL(*ib, 42);
			ASSERT_EQUAL(StatefulDeleter<int>::s_state, 0);
		}
		ASSERT_EQUAL(StatefulDeleter<int>::s_state, 1);

		static_assert(
			not std::is_constructible_v<Box<int, StatefulDeleter<int>>, Box<int>>,
			"Box with custom deleter should not be constructible from Box with default deleter"
		);

		{
			auto ib = Box<int, StatefulDeleter<int>>::fromPointerWithCustomDeleter(
				new int(42), StatefulDeleter<int>{ 7 }
			);
			Box<int, FromStatefulDeleterByValue<int>> jb = std::move(ib);
		}
		{
			auto ib = Box<int, StatefulDeleter<int>>::fromPointerWithCustomDeleter(
				new int(42), StatefulDeleter<int>{ 7 }
			);
			Box<int, FromStatefulDeleterByCopy<int>> jb = std::move(ib);
		}
		{
			auto ib = Box<int, StatefulDeleter<int>>::fromPointerWithCustomDeleter(
				new int(42), StatefulDeleter<int>{ 7 }
			);
			Box<int, FromStatefulDeleterByMove<int>> jb = std::move(ib);
		}
		ASSERT_EQUAL(StatefulDeleter<int>::s_state, 1);

		{
			MBox ib = Box<int, StatefulDeleter<int>>::fromPointerWithCustomDeleter(
				new int(42), StatefulDeleter<int>{ 7 }
			);
			Box b = std::move(ib).toOptBox().value();
		}
		ASSERT_EQUAL(StatefulDeleter<int>::s_state, 2);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
