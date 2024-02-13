/**
 * @file optional_test.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <tester/tester.hpp>
#include <base/optional.hpp>
#include <queue>

#define SPACESHIP_PASTE(o1, o2)               \
	if (a < b) ASSERT_EQUAL(true, o1 < o2);   \
	if (a == b) ASSERT_EQUAL(true, o1 == o2); \
	if (a <= b) ASSERT_EQUAL(true, o1 <= o2); \
	if (a > b) ASSERT_EQUAL(true, o1 > o2);   \
	if (a != b) ASSERT_EQUAL(true, o1 != o2); \
	if (a >= b) ASSERT_EQUAL(true, o1 >= o2);


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
		TESTER_ADD_TEST(testFromDocs);
		TESTER_ADD_TEST(assignTest);
		TESTER_ADD_TEST(swapTest);
		TESTER_ADD_TEST(danglingPointerTest);
		TESTER_ADD_TEST(comparatorTest);
		TESTER_ADD_TEST(boolAndResetTest);
		TESTER_ADD_TEST(arrowOperatorTest);
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

	void assignTest() {
		base::Optional<int> a;
		a = 1;
		assert(1 == *a, "A not equal to 1");
		a = 2;
		assert(2 == *a, "A not equal to 2");

		std::string                 str1 = "str1", str2 = "str2";
		base::Optional<std::string> b;
		b = str1;
		assert(str1 == *b, "B not equal to str1");
		b = str2;
		assert(str2 == *b, "B not equal to str2");

		base::Optional<std::string&> c;
		c = str1;
		assert(str1 == *c, "C not equal to str1");
		c = str2;
		assert(str2 == *c, "C not equal to str2");
		c.value()[0] = 'd';
		assert(str2[0] == 'd', "C should start with a d");
	}

	void swapTest() {
		base::Optional<std::string> a = "1";
		base::Optional<std::string> b = "2";
		std::swap(a, b);
		assert("2" == *a, "a does not hold 2");
		assert("1" == *b, "b does not hold 1");

		std::string                  str1 = "123";
		std::string                  str2 = "321";
		base::Optional<std::string&> c    = str1;
		base::Optional<std::string&> d    = str2;
		std::swap(c, d);
		assert(str2 == *c, "c does not hold str2");
		assert(str1 == *d, "d does not hold str1");
	}

	void danglingPointerTest() {
		// This would not compile if -Werror flag is on, and it would create a dangling pointer...
		if_opt_some(Optional(1), val) { assert(val == 1, base::strConcat("val != 1, but: ", val)); }
	}

	void testFromDocs() {
		// Create an empty optional
		base::Optional<int> opt;

		ASSERT_EQUAL(false, opt.has_value());
		// or simply
		ASSERT_EQUAL(true, opt.empty());

		opt = 1;
		ASSERT_EQUAL(1, *opt);
		ASSERT_EQUAL(1, opt.value());

		base::Optional<int> opt2(2);
		// Mapping the value, and changing a type!
		ASSERT_EQUAL(2.2, *opt2.map([](int v) { return v * 1.1; }));

		// --------------------------------------------------

		// base::Optional can also hold a reference!
		std::string                  name = "Rift";
		base::Optional<std::string&> opt_name(name);

		opt_name.value().push_back('!');
		ASSERT_EQUAL("Rift!", name);
	}

	void comparatorTest() {
		int a = 1;
		int b = 2;
		// Reference
		for (int i = 0; i < 3; i++) {
			base::Optional<int&> o1 = a;
			base::Optional<int&> o2 = b;
			SPACESHIP_PASTE(o1, o2);
			a++;
		}

		a = 1;
		// No reference
		for (int i = 0; i < 3; i++) {
			base::Optional<int> o1 = a;
			base::Optional<int> o2 = b;
			SPACESHIP_PASTE(o1, o2);
			a++;
		}
	}

	void boolAndResetTest() {
		base::Optional<int> o = 1;
		ASSERT_EQUAL(true, o.has_value());
		ASSERT_EQUAL(true, bool(o));
		o.reset();
		ASSERT_EQUAL(false, o.has_value());

		base::Optional<int> o2 = 1;
		ASSERT_EQUAL(true, o2.has_value());
		ASSERT_EQUAL(true, bool(o2));
		o2.reset();
		ASSERT_EQUAL(false, o2.has_value());
	}

	void arrowOperatorTest() {
		base::Optional<std::string> opt = "";
		opt->push_back('c');
		ASSERT_EQUAL("c", opt.value());

		std::string                  str;
		base::Optional<std::string&> opt_ref(str);
		opt_ref->push_back('r');
		ASSERT_EQUAL("r", opt_ref.value());
	}
};

TESTER_COMMON_MAIN("/common/base/tests/");
