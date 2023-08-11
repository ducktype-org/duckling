#include <tester/tester.hpp>


class SimpleTesterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleTesterTest
	i32 test_no;
public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Tester Test", i32 test_no) {
		this->test_no = test_no;
		TESTER_ADD_TEST(choose);
	}

	~SimpleTesterTest() override = default;

private:

	void choose() {
		switch (test_no) {
			case 0:
				return happy();
			case 1:
				return failing_is_not_throwing_std();
			case 2:
				return catch_no_throw();
			case 3:
				return catch_wrong_throw();
			default: 
				return;
		}
	}

	void happy() {
		assertThrows<std::logic_error>([&](){throw std::logic_error("Hi!");}, "Logic Error");

		assertThrows<std::exception>([&](){throw std::exception();}, "std::exception");

		assertThrows<i32>([&](){throw 3;}, "int");
	}

	void failing_is_not_throwing_std() {
		// Expected to fail: checks that failing a test is not mistaken for throwing std::exception.
		try{
			assertThrows<std::exception>([&](){ fail("Please wait patiently for the failure of the system...");}, "This message is unfortunately discarded.");
		} catch (const tester::TestSuite::CritTestError& e) {
			message("Task failed successfully.");
		}
	}

	void catch_no_throw() {
		message("Expected to fail: checks that assertThrows fails when no exception is caught");
		assertThrows<std::logic_error>([&](){}, "expected failure: No exception was thrown");
	}

	void catch_wrong_throw() {
		message("Expected to fail: checks that assertThrows fails when wrong exception is caught.");
		assertThrows<std::logic_error>([&](){throw std::exception();}, "expected failure: Wrong exception was thrown");	
	}
};


int main(int argc, char** argv) {
	auto config = tester::testConfigFromArgs(
		{argc, argv},
		"/common/tester/tests/"
	);

	SimpleTesterTest passing_test(std::move(config), 0);
	if (!passing_test.run())
		return 1;

	/* The tests below are "expected to fail".
	 * Currently there is no way to specify that. 
	 * @TODO: change that when "expected to fail" is added
	 */ 
	for(i32 i = 1 ; i < 4; i++) {
		SimpleTesterTest failing_test(std::move(config), i);
		if (failing_test.run())
			return 1;
	}
}
