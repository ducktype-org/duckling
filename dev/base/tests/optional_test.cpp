/**
 * @file optional_test.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <tester/tester.hpp>
#include <base/optional.hpp>
#include <queue>

class OptionalTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OptionalTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("base::Optional<T> test") {
		TESTER_ADD_TEST(basicTest);
		TESTER_ADD_TEST(mapTest);
		TESTER_ADD_TEST(flatMapTest);
	}

	void basicTest() {
		using base::Optional;

		base::Optional<int> opt(4);
		assert(*opt == 4, "Value not equal to 4");
		match_optional(opt) {
			opt_some(val) assert(val == 4, "Value not equal to 4");
			opt_none assert(false, "Entered None case with value");
		}

		base::Optional<int> opt2;
		match_optional(opt2) {
			opt_some(_val) {
				(void) _val;
				assert(false, "No value, in opt2, shouldn't enter this case");
			}
			opt_none assert(opt2.empty(), "Entered opt_none with a value!");
		}

		base::Optional<int> opt3;
		assert(opt3.empty(), "Test3 should hold no value");
		opt3 = base::Optional(3);
		assert(opt3.has_value(), "Test3 should have no value");
		assert(opt3.value() == 3, "Test3 should have value of 3");
	}

	void mapTest() {
		using base::Optional;
		Optional<int> opt(4);

		auto result = opt.map([](int val) { return val * 1.1; });
		assert(result.value() == 4.4, base::strConcat("Result is not 4.4, but: ", result.value()));

		auto result2 = opt.map([](int val) { return val * val; });
		assert(result2.value() == 16, base::strConcat("Result is not 16, but: ", result2.value()));

		Optional<int> empty;
		auto          result3 = empty.map([](int val) { return val * 2; });
		assert(result3.empty(), "Result3 is not empty!");
	}

	void flatMapTest() {
		using base::Optional;
		Optional<int> opt(4);
		auto          result = opt.flatMap([](int val) { return Optional(val * 1.1); });
		assert(result.value() == 4.4, base::strConcat("Result is not 4.4, but: ", result.value()));

		Optional<int> empty;
		auto          result2 = empty.flatMap([](auto val) { return Optional(val * 2); });
		assert(result2.empty(), "Result2 is not empty!");
	}
};

TESTER_COMMON_MAIN("/common/flag_type/tests/");
