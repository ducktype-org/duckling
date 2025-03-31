/**
 * @file optional_test.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <base/optional.hpp>

#include <tester/tester.hpp>

using base::Optional;

class OptionalTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OptionalTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
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
		TESTER_ADD_TEST(ifOptSomeTest);
	}

	template<class T, class U>
	void spaceshipPaste(T& a, T& b, base::Optional<U>& o1, base::Optional<U>& o2) {
		if (a < b) ASSERT_EQUAL(true, o1 < o2);
		if (a == b) ASSERT_EQUAL(true, o1 == o2);
		if (a <= b) ASSERT_EQUAL(true, o1 <= o2);
		if (a > b) ASSERT_EQUAL(true, o1 > o2);
		if (a != b) ASSERT_EQUAL(true, o1 != o2);
		if (a >= b) ASSERT_EQUAL(true, o1 >= o2);
	}

	void basicTest() {
		Optional<int> opt(4);
		ASSERT_EQUAL(*opt, 4);

		Optional<int> opt2;
		ASSERT_TRUE(opt2.empty());
		opt2 = base::Optional(2);
		ASSERT_TRUE(opt2.has_value());
		ASSERT_EQUAL(opt2.value(), 2);
	}

	void macroTest() {
		Optional<int> opt(4);

		bool visited = false;
		match_optional(opt) {
			opt_some(val) {
				ASSERT_EQUAL(val, 4);
				visited = true;
			}
			opt_none assertTrue(false, "Entered None case with value");
		}
		assertTrue(visited, "Macro opt_some does not work");

		visited = false;
		if_opt_some(opt, val) {
			ASSERT_EQUAL(val, 4);
			visited = true;
		}
		assertTrue(visited, "Macro if_opt_some does not work");

		if_opt_none(opt) assertTrue(false, "opt has value, shouldn't enter this case");


		Optional<int> opt2;
		match_optional(opt2) {
			opt_some(_val) {
				(void) _val;  // So that the compiler doesn't yell at us for not using the value.
				assertTrue(false, "No value, in opt2, shouldn't enter this case");
			}
			opt_none assertTrue(opt2.empty(), "Entered opt_none with a value!");
		}

		visited                   = false;
		if_opt_none(opt2) visited = true;
		assertTrue(visited, "Macro if_opt_none does not work");
		if_opt_some(opt2, value) {
			(void) value;
			assertTrue(false, "No value, in opt2, shouldn't enter this case");
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
		ASSERT_EQUAL(result.value(), 4.4);

		auto result2 = opt.map([](int val) { return val * val; });
		ASSERT_EQUAL(result2.value(), 16);

		Optional<int> empty;
		auto          result3 = empty.map([](int val) { return val * 2; });
		assertTrue(result3.empty(), "Result3 is not empty!");
	}

	void flatMapTest() {
		Optional<int> opt(4);
		auto          result = opt.flatMap([](int val) { return Optional(val * 1.1); });
		ASSERT_EQUAL(result.value(), 4.4);

		Optional<int> empty;
		auto          result2 = empty.flatMap([](auto val) { return Optional(val * 2); });
		assertTrue(result2.empty(), "Result2 is not empty!");
	}

	void testReference() {
		std::vector<int>            vec = { 1, 2, 3 };
		Optional<std::vector<int>&> optVec(vec);
		optVec.value()[0]++;
		vec[1] = 30;
		for (usize i = 0; i < vec.size(); i++) {
			assertTrue(
				vec[i] == optVec.value()[i],
				base::strConcat("Values at index ", i, " are not equal, but it's a reference.")
			);
		}
	}

	void assignTest() {
		base::Optional<int> a;
		a = 1;
		ASSERT_EQUAL(1, *a);
		a = 2;
		ASSERT_EQUAL(2, *a);

		std::string                 str1 = "str1", str2 = "str2";
		base::Optional<std::string> b;
		b = str1;
		ASSERT_EQUAL(str1, *b);
		b = str2;
		ASSERT_EQUAL(str2, *b);

		base::Optional<std::string&> c;
		c = str1;
		ASSERT_EQUAL(str1, *c);
		c = str2;
		ASSERT_EQUAL(str2, *c);
		c.value()[0] = 'd';
		ASSERT_EQUAL(str2[0], 'd');
	}

	void swapTest() {
		base::Optional<std::string> a = "1";
		base::Optional<std::string> b = "2";
		std::swap(a, b);
		ASSERT_EQUAL("2", *a);
		ASSERT_EQUAL("1", *b);

		std::string                  str1 = "123";
		std::string                  str2 = "321";
		base::Optional<std::string&> c    = str1;
		base::Optional<std::string&> d    = str2;
		std::swap(c, d);
		ASSERT_EQUAL(str2, *c);
		ASSERT_EQUAL(str1, *d);
	}

	void danglingPointerTest() {
		// This would not compile if -Werror flag is on, and it would create a dangling pointer...
		if_opt_some(Optional(1), val) { ASSERT_EQUAL(val, 1); }
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
		std::string                  name = "Duckling";
		base::Optional<std::string&> opt_name(name);

		opt_name.value().push_back('!');
		ASSERT_EQUAL("Duckling!", name);
	}

	void comparatorTest() {
		int a = 1;
		int b = 2;
		// Reference
		for (int i = 0; i < 3; i++) {
			base::Optional<int&> o1 = a;
			base::Optional<int&> o2 = b;
			spaceshipPaste(a, b, o1, o2);
			a++;
		}

		a = 1;
		// No reference
		for (int i = 0; i < 3; i++) {
			base::Optional<int> o1 = a;
			base::Optional<int> o2 = b;
			spaceshipPaste(a, b, o1, o2);
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

	void ifOptSomeTest() {
		base::Optional<int> opt = 1;
		{
			bool entered = false;
			if_opt_some(opt, val) {
				ASSERT_EQUAL(1, val);
				entered = true;
			}
			ASSERT_TRUE(entered);

			if_opt_none(opt) { fail("Entered if_opt_none with value!"); }
		}

		opt = std::nullopt;
		{
			bool entered = false;
			if_opt_none(opt) { entered = true; }
			ASSERT_TRUE(entered);

			if_opt_some(opt, _) { fail("Entered if_opt_some with no value!"); }
		}

		{
			u64  count   = 0;
			auto get_opt = [&]() -> base::Optional<int> {
				count++;
				return 1;
			};
			if_opt_some(get_opt(), val) { ASSERT_EQUAL(1, val); }
			ASSERT_EQUAL(1, count);
		}
	}
};

TESTER_COMMON_MAIN("/base/tests/");
