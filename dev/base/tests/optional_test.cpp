/**
 * @file optional_test.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <tester/tester.hpp>
#include <base/optional.hpp>
#include <queue>

using base::Optional;

class OptionalTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OptionalTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("base::Optional<T> test") {
		TESTER_ADD_TEST(basicTest);
		TESTER_ADD_TEST(mapTest);
		TESTER_ADD_TEST(flatMapTest);
		TESTER_ADD_TEST(testReference);
		TESTER_ADD_TEST(macroTest);
		TESTER_ADD_TEST(throwTest);
	}

	void basicTest() {
		Optional<int> opt(4);
		assert(*opt == 4, "Value not equal to 4");

		Optional<int> opt2;
		assert(opt2.empty(), "opt2 should hold no value");
		opt2 = base::Optional(2);
		assert(opt2.has_value(), "opt2 should have no value");
		assert(opt2.value() == 2, "opt2 should have value of 2");
	}

	void macroTest() {
		Optional<int> opt(4);

		bool visited = false;
		match_optional(opt) {
			opt_some(val) {
				assert(val == 4, "Value not equal to 4");
				visited = true;
			}
			opt_none assert(false, "Entered None case with value");
		}
		assert(visited, "Macro opt_some does not work");

		visited = false;
		if_opt_some(opt, val) {
			assert(val == 4, "Value not equal to 4");
			visited = true;
		}
		assert(visited, "Macro if_opt_some does not work");

		if_opt_none(opt) assert(false, "opt has value, shouldn't enter this case");


		Optional<int> opt2;
		match_optional(opt2) {
			opt_some(_val) {
				(void) _val;  // So that the compiler doesn't yell at us for not using the value.
				assert(false, "No value, in opt2, shouldn't enter this case");
			}
			opt_none assert(opt2.empty(), "Entered opt_none with a value!");
		}

		visited                   = false;
		if_opt_none(opt2) visited = true;
		assert(visited, "Macro if_opt_none does not work");
		if_opt_some(opt2, value) {
			(void) value;
			assert(false, "No value, in opt2, shouldn't enter this case");
		}
	}

	void throwTest() {
		Optional<int> opt;
		assertThrows<std::exception>(
			[&]() { (void) opt.value(); }, "Optional does not throw on no value access!"
		);
	}

	void mapTest() {
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
		Optional<int> opt(4);
		auto          result = opt.flatMap([](int val) { return Optional(val * 1.1); });
		assert(result.value() == 4.4, base::strConcat("Result is not 4.4, but: ", result.value()));

		Optional<int> empty;
		auto          result2 = empty.flatMap([](auto val) { return Optional(val * 2); });
		assert(result2.empty(), "Result2 is not empty!");
	}

	void testReference() {
		std::vector<int>            vec = { 1, 2, 3 };
		Optional<std::vector<int>&> optVec(vec);
		optVec.value()[0]++;
		vec[1] = 30;
		for (usize i = 0; i < vec.size(); i++) {
			assert(
				vec[i] == optVec.value()[i],
				base::strConcat("Values at index ", i, " are not equal, but it's a reference.")
			);
		}
	}
};

TESTER_COMMON_MAIN("/common/flag_type/tests/");
