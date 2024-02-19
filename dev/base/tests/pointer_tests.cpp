#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <base/smart_pointers.hpp>
#include <sstream>

struct Wrapper {
	i32 x{};

	Wrapper() = default;

	Wrapper(i32 x): x(x) {}

	virtual ~Wrapper() = default;
};

struct Derived: public Wrapper {
	static i32 destructor_count;

	~Derived() override { destructor_count++; }
};

i32 Derived::destructor_count = 0;

class OwnershipPointerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OwnershipPointerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Ownership pointer test") {
		TESTER_ADD_TEST(testPassByValue);
		TESTER_ADD_TEST(testOwnership);
	}

private:
	void testPassByValue() {
		auto u_ptr = base::make_unique<Wrapper>(3);
		increment_wrapper_using_borrow(u_ptr.borrow_mut());
		assert(u_ptr->x == 4, "Pass by value failed");
	}

	void testOwnership() {
		auto u_ptr = base::make_unique<Wrapper>(1);
		{
			auto b_ptr = u_ptr.borrow_mut();
			b_ptr->x   = 5;
		}
		assert(u_ptr->x == 5, "Scoped borrow assignment failed");

		auto b_ptr     = u_ptr.borrow();
		auto u_ptr_raw = u_ptr.release();
		assert(u_ptr.get() == nullptr, "release didn't worked properly (1)");
		assert(b_ptr.get() == u_ptr_raw, "release didn't worked properly (2)");
		delete u_ptr_raw;

		// compile time method checks
		base::unique_ptr<Derived> n_ptr1 = nullptr;
		base::unique_ptr<Wrapper> sth1   = std::move(n_ptr1);
		n_ptr1                           = nullptr;
		sth1                             = std::move(n_ptr1);

		// ownership check:
		auto d_count = Derived::destructor_count;
		assert(Derived::destructor_count == d_count, "UB");

		{
			base::unique_ptr<Derived> d_ptr(new Derived());

			{
				d_ptr.borrow();
				d_ptr.borrow_mut();
			}

			assert(Derived::destructor_count == d_count, "Borrows destroy the object");
		}

		assert(Derived::destructor_count == d_count + 1, "Unique didn't destroy the object");
	}

	void increment_wrapper_using_borrow(base::borrow_ptr<Wrapper> b_ptr) { b_ptr->x++; }
};

TESTER_COMMON_MAIN("/base/tests/");
