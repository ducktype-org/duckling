/**
 * @file optional_test.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

#include <expected>
#include <sstream>

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
		TESTER_ADD_TEST(comparatorTest);
		TESTER_ADD_TEST(boolAndResetTest);
		TESTER_ADD_TEST(arrowOperatorTest);
		TESTER_ADD_TEST(ifOptSomeTest);
		TESTER_ADD_TEST(testMatchErr);
		TESTER_ADD_TEST(testExpect);
		TESTER_ADD_TEST(testPreservingValueCategories);
		TESTER_ADD_TEST(testAddresses);
		TESTER_ADD_TEST(testCopyValueOr);
		TESTER_ADD_TEST(testCopyValueOrElse);
		TESTER_ADD_TEST(testMapOr);
		TESTER_ADD_TEST(testFilter);
		TESTER_ADD_TEST(testOrElse);
		TESTER_ADD_TEST(testInspect);
		TESTER_ADD_TEST(testFlatten);
		TESTER_ADD_TEST(testOkOr);
		TESTER_ADD_TEST(testTakeAndReplace);
		TESTER_ADD_TEST(testGetOrInsert);
	}

	template<typename X>
	struct is_ref: std::false_type {};

	template<typename Y>
	struct is_ref<Ref<Y>>: std::true_type {};

	template<typename T, typename U>
	void spaceshipPaste(T& a, T& b, base::Optional<U>& o1, base::Optional<U>& o2) {
		auto get_val = [](auto& opt) -> decltype(auto) {
			if constexpr (is_ref<U>::value)
				return *opt.value();
			else
				return opt.value();
		};

		if (a < b) ASSERT_EQUAL(true, get_val(o1) < get_val(o2));
		if (a == b) ASSERT_EQUAL(true, get_val(o1) == get_val(o2));
		if (a <= b) ASSERT_EQUAL(true, get_val(o1) <= get_val(o2));
		if (a > b) ASSERT_EQUAL(true, get_val(o1) > get_val(o2));
		if (a != b) ASSERT_EQUAL(true, get_val(o1) != get_val(o2));
		if (a >= b) ASSERT_EQUAL(true, get_val(o1) >= get_val(o2));
	}

	void basicTest() {
		Optional<int> opt(4);
		ASSERT_EQUAL(*opt, 4);

		Optional<int> opt2;
		ASSERT_TRUE(opt2.empty());
		opt2 = base::Optional(2);
		ASSERT_HAS_VALUE(opt2);
		ASSERT_EQUAL(opt2.value(), 2);

		opt2 = opt;
		ASSERT_HAS_VALUE(opt);
		ASSERT_HAS_VALUE(opt2);
		ASSERT_EQUAL(opt.value(), 4);
		ASSERT_EQUAL(opt2.value(), 4);

		opt2.value() = 3;
		ASSERT_HAS_VALUE(opt);
		ASSERT_HAS_VALUE(opt2);
		ASSERT_EQUAL(opt.value(), 4);
		ASSERT_EQUAL(opt2.value(), 3);
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
			opt_some(_) { assertTrue(false, "No value, in opt2, shouldn't enter this case"); }
			opt_none assertTrue(opt2.empty(), "Entered opt_none with a value!");
		}

		visited                   = false;
		if_opt_none(opt2) visited = true;
		assertTrue(visited, "Macro if_opt_none does not work");
		if_opt_some(opt2, _) { assertTrue(false, "No value, in opt2, shouldn't enter this case"); }
	}

	void throwTest() {
		Optional<int> opt;
		assertThrows<std::exception>(
			[&]() { std::ignore = opt.value(); }, "Optional does not throw on no value access!"
		);
	}

	struct MemberFunction {
		int value;

		// Making the type non-copyable
		MemberFunction(int v): value{ v } {}

		MemberFunction(const MemberFunction&)            = delete;
		MemberFunction& operator=(const MemberFunction&) = delete;

		Optional<int> memberFunctionL() & { return value; }

		Optional<int> memberFunctionR() && { return 2 * value; }
	};

	void mapTest() {
		Optional<int> opt(4);

		auto result = opt.map([](int val) { return val * 1.1; });
		ASSERT_EQUAL(result.value(), 4.4);

		auto result2 = opt.map([](int val) { return val * val; });
		ASSERT_EQUAL(result2.value(), 16);

		Optional<int> empty;
		auto          result3 = empty.map([](int val) { return val * 2; });
		assertTrue(result3.empty(), "Result3 is not empty!");

		Optional<MemberFunction> opt2{ 10 };
		auto                     result4 = opt2.map(&MemberFunction::memberFunctionL);
		ASSERT_EQUAL(result4.value(), 10);

		auto result5 = std::move(opt2).map(&MemberFunction::memberFunctionR);
		ASSERT_EQUAL(result5.value(), 20);
	}

	void flatMapTest() {
		Optional<int> opt(4);
		auto          result = opt.flatMap([](int val) { return Optional(val * 1.1); });
		ASSERT_EQUAL(result.value(), 4.4);

		Optional<int> empty;
		auto          result2 = empty.flatMap([](auto val) { return Optional(val * 2); });
		assertTrue(result2.empty(), "Result2 is not empty!");

		Optional<MemberFunction> opt2{ 10 };
		auto                     result3 = opt2.flatMap(&MemberFunction::memberFunctionL);
		ASSERT_EQUAL(result3.value(), 10);

		auto result4 = std::move(opt2).flatMap(&MemberFunction::memberFunctionR);
		ASSERT_EQUAL(result4.value(), 20);
	}

	void testReference() {
		std::vector<int>                      vec = { 1, 2, 3 };
		Optional<base::Ref<std::vector<int>>> opt_vec(&vec);
		(*opt_vec.value())[0]++;
		vec[1] = 30;
		for (usize i = 0; i < vec.size(); i++) {
			assertTrue(
				vec[i] == (*opt_vec.value())[i],
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

		base::Optional<base::Ref<std::string>> c;
		c = &str1;
		ASSERT_EQUAL(str1, *c.value());
		c = &str2;
		ASSERT_EQUAL(str2, *c.value());
		(*c.value())[0] = 'd';
		ASSERT_EQUAL(str2[0], 'd');
	}

	void swapTest() {
		base::Optional<std::string> a = "1";
		base::Optional<std::string> b = "2";
		std::swap(a, b);
		ASSERT_EQUAL("2", *a);
		ASSERT_EQUAL("1", *b);

		std::string                            str1 = "123";
		std::string                            str2 = "321";
		base::Optional<base::Ref<std::string>> c    = &str1;
		base::Optional<base::Ref<std::string>> d    = &str2;
		std::swap(c, d);
		ASSERT_EQUAL(str2, *c.value());
		ASSERT_EQUAL(str1, *d.value());
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
		std::string                            name = "Duckling";
		base::Optional<base::Ref<std::string>> opt_name(&name);

		opt_name.value()->push_back('!');
		ASSERT_EQUAL("Duckling!", name);
	}

	void comparatorTest() {
		int a = 1;
		int b = 2;
		// Reference
		for (int i = 0; i < 3; i++) {
			base::Optional<base::Ref<int>> o1 = &a;
			base::Optional<base::Ref<int>> o2 = &b;
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

		std::string                            str;
		base::Optional<base::Ref<std::string>> opt_ref(&str);
		opt_ref.value()->push_back('r');
		ASSERT_EQUAL("r", *opt_ref.value());
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

	void testMatchErr() {
		{
			std::expected<int, float> opt;
			opt = 1;
			match_optional(opt) {
				opt_some(v) ASSERT_EQUAL(1, v);
				opt_err(er [[maybe_unused]]) CORE_PANIC("Invalid path");
			}
			opt = std::unexpected(0.1f);
			match_optional(opt) {
				opt_some(v [[maybe_unused]]) CORE_PANIC("Invalid path");
				opt_err(er) ASSERT_EQUAL(0.1f, er);
			}
		}

		{
			struct A {
				int val;

				A(int val): val(val) {}

				A(A&&)            = default;
				A& operator=(A&&) = default;

				A(const A&)            = delete;
				A& operator=(const A&) = delete;
			};

			std::expected<float, A> opt = 5.5f;
			match_optional(opt) {
				opt_some_move(v) ASSERT_EQUAL(5.5f, v);
				opt_err(a [[maybe_unused]]) fail("Invalid branch");
			}
			opt = std::unexpected<A>(1);
			match_optional(opt) {
				opt_some_move(v [[maybe_unused]]) fail("Invalid branch");
				opt_err(a) ASSERT_EQUAL(1, a.val);
			}
		}
	}

	void testExpect() {
		try {
			std::ignore = Optional<base::Ref<float>>().expect<int>(21);
			fail("No throw");
		} catch (int er) { ASSERT_EQUAL(er, 21); }

		base::Optional<float> empty;
		try {
			std::ignore = empty.expect<int>(42);
			fail("No throw");
		} catch (int er) { ASSERT_EQUAL(er, 42); }
	}

	template<typename OptionalT, typename ExpectedValueT>
	void assertValueT() {
#define ASSERT_EQUAL_TYPES(type1, type2) ASSERT_TRUE((std::same_as<type1, type2>) )
#define optional                         std::declval<OptionalT>()
#define expected_value                   std::declval<ExpectedValueT>()

		ASSERT_EQUAL_TYPES(decltype(optional.value()), ExpectedValueT);
		ASSERT_EQUAL_TYPES(decltype(*optional), ExpectedValueT);
		ASSERT_EQUAL_TYPES(decltype(optional.expect("")), ExpectedValueT);
		ASSERT_EQUAL_TYPES(decltype(optional.template expect<int>(5)), ExpectedValueT);

#undef expected_value
#undef optional
#undef ASSERT_EQUAL_TYPES
	}

	void testPreservingValueCategories() {
		// Value is an x-value
		assertValueT<Optional<int>&&, int&&>();
		assertValueT<const Optional<int>&&, const int&&>();

		// Value is an l-value
		assertValueT<Optional<int>&, int&>();
		assertValueT<const Optional<int>&, const int&>();
	}

	void testAddresses() {
		std::string test_brief = "Divergent address of";
		std::string state;

		auto assert_equal_addr = [&](u32& expected, u32& got, const std::string& message) {
			assertEqual(&expected, &got, test_brief + ": " + message + ", " + state);
		};

		Optional<u32> optional{ 21 };
		u32&          internal_integer = optional.value();
		assert_equal_addr(internal_integer, optional.value(), "value");
		assert_equal_addr(internal_integer, *optional, "*operator");
		assert_equal_addr(internal_integer, optional.expect(""), "expect with message");
		assert_equal_addr(internal_integer, optional.expect<std::string>(""), "expect with error");

		optional = Optional<u32>{ 37 };
		state    = "after copy";
		assert_equal_addr(internal_integer, optional.value(), "value");
		assert_equal_addr(internal_integer, *optional, "operator*");
		assert_equal_addr(internal_integer, optional.expect(""), "expect with message");
		assert_equal_addr(internal_integer, optional.expect<std::string>(""), "expect with error");
	}

	/**
	 * @struct CtrAssignCounter
	 * @brief Represents the number of times the constructors and assignments where invoked.
	 */
	struct CtrAssignCounter {
		u32 value_constructed = 0;
		u32 move_constructed  = 0;
		u32 copy_constructed  = 0;
		u32 move_assigned     = 0;
		u32 copy_assigned     = 0;
		u32 destructed        = 0;

		void reset() { *this = CtrAssignCounter{}; }
	};

	/**
	 * @struct CountCtrStruct
	 * @brief Holds a u32 value and counts how many times each constructor was invoked.
	 */
	struct CountCtrStruct {
		static CtrAssignCounter& counter() {
			static CtrAssignCounter counter{};
			return counter;
		}

		auto operator<=>(const CountCtrStruct&) const = default;

		u32 value;

		CountCtrStruct(u32 value) noexcept: value{ value } { ++counter().value_constructed; }

		CountCtrStruct(CountCtrStruct&& other) noexcept: value{ other.value } {
			++counter().move_constructed;
		}

		CountCtrStruct(const CountCtrStruct& other) noexcept: value{ other.value } {
			++counter().copy_constructed;
		}

		CountCtrStruct& operator=(const CountCtrStruct& other) & noexcept {
			value = other.value;
			++counter().copy_assigned;
			return *this;
		}

		CountCtrStruct& operator=(CountCtrStruct&& other) & noexcept {
			value = other.value;
			++counter().move_assigned;
			return *this;
		}

		~CountCtrStruct() noexcept { ++counter().destructed; }
	};

	/**
	 * @brief Checks one-by-one if each constructor and assignment was invoked exactly the
	 * number of times it was expected to be, printing an informing message on error.
	 * After that it resets the counts.
	 */
	void checkCountsAndReset(CtrAssignCounter counter, std::string description) {
		auto details = [](u32 expected, u32 got) -> std::string {
			return (std::stringstream{} << "expected: " << expected << ", but got: " << got).str();
		};

		auto check_counter = [&](u32 CtrAssignCounter::* value, const std::string& name) {
			assertEqual(
				counter.*value,
				CountCtrStruct::counter().*value,
				"Unexpected " + name + " (" + description + ") "
					+ details(counter.*value, CountCtrStruct::counter().*value)
			);
		};
		check_counter(&CtrAssignCounter::value_constructed, "value constructor");
		check_counter(&CtrAssignCounter::copy_constructed, "copy constructor");
		check_counter(&CtrAssignCounter::move_constructed, "move constructor");
		check_counter(&CtrAssignCounter::copy_assigned, "copy assignment");
		check_counter(&CtrAssignCounter::move_assigned, "move assignment");
		check_counter(&CtrAssignCounter::destructed, "destructed");

		CountCtrStruct::counter().reset();
	}

	void testCopyValueOr() {
		// Testing if copyValueOr does not return a dangling reference.
		CountCtrStruct::counter().reset();
		auto&& result1 = Optional<CountCtrStruct>{ 3 }.copyValueOr(CountCtrStruct{ 4 });
		checkCountsAndReset(
			CtrAssignCounter{
				.value_constructed = 2,
				.move_constructed  = 1,
				.destructed        = 2,
			},
			"copyValueOr on non-empty r-value"
		);
		assertEqual(CountCtrStruct{ 3 }, result1, "copyValueOr failed (non-empty r-value)");

		CountCtrStruct::counter().reset();
		auto&& result2 = Optional<CountCtrStruct>{}.copyValueOr(CountCtrStruct{ 4 });
		checkCountsAndReset(
			CtrAssignCounter{
				.value_constructed = 1,
				.move_constructed  = 1,
				.destructed        = 1,
			},
			"copyValueOr on empty r-value"
		);
		assertEqual(CountCtrStruct{ 4 }, result2, "copyValueOr failed (empty r-value)");

		// Testing if just passing constructor arguments will suffice.
		Optional<CountCtrStruct> optional{};
		assertEqual(
			CountCtrStruct{ 4 }, optional.copyValueOr({ 4 }), "copyValueOr failed (empty r-value)"
		);

		CountCtrStruct value{ 5 };
		assertEqual(
			CountCtrStruct{ 5 },
			Optional<CountCtrStruct>{}.copyValueOr(value),
			"copyValueOr failed (empty r-value)"
		);
	}

	void testCopyValueOrElse() {
		u64 calls = 0;
		int base  = 41;
		// The fallback takes no arguments, exactly like Rust's unwrap_or_else: whatever it needs
		// is captured.
		auto make = [&] {
			calls++;
			return base + 1;
		};

		Optional<int> opt = 1;
		ASSERT_EQUAL(1, opt.copyValueOrElse(make));
		// This is the whole point of the method: a non-empty optional never runs the fallback.
		ASSERT_EQUAL(u64{ 0 }, calls);

		Optional<int> empty;
		ASSERT_EQUAL(42, empty.copyValueOrElse(make));
		ASSERT_EQUAL(u64{ 1 }, calls);

		ASSERT_EQUAL(0, empty.copyValueOrDefault());
		ASSERT_EQUAL(1, opt.copyValueOrDefault());

		// Skipping the fallback also means skipping the objects it would have built.
		CountCtrStruct::counter().reset();
		auto&& result
			= Optional<CountCtrStruct>{ 3 }.copyValueOrElse([] { return CountCtrStruct{ 4 }; });
		checkCountsAndReset(
			CtrAssignCounter{
				.value_constructed = 1,
				.move_constructed  = 1,
				.destructed        = 1,
			},
			"copyValueOrElse on non-empty r-value"
		);
		assertEqual(CountCtrStruct{ 3 }, result, "copyValueOrElse failed (non-empty r-value)");
	}

	void testMapOr() {
		auto twice = [](int value) { return value * 2; };

		Optional<int> opt = 4;
		Optional<int> empty;
		ASSERT_EQUAL(8, opt.mapOr(-1, twice));
		ASSERT_EQUAL(-1, empty.mapOr(-1, twice));

		u64  calls    = 0;
		auto fallback = [&]() {
			calls++;
			return -1;
		};
		ASSERT_EQUAL(8, opt.mapOrElse(fallback, twice));
		ASSERT_EQUAL(u64{ 0 }, calls);
		ASSERT_EQUAL(-1, empty.mapOrElse(fallback, twice));
		ASSERT_EQUAL(u64{ 1 }, calls);

		// The mapping function may change the type, just like map does.
		ASSERT_EQUAL(4.4, opt.mapOr(0.0, [](int value) { return value * 1.1; }));
	}

	void testFilter() {
		auto is_even = [](int value) { return value % 2 == 0; };

		Optional<int> opt = 4;
		ASSERT_EQUAL(4, *opt.filter(is_even));
		ASSERT_TRUE(opt.filter([](int value) { return value % 2 == 1; }).empty());

		Optional<int> empty;
		ASSERT_TRUE(empty.filter(is_even).empty());

		// A predicate taking a reference must not consume the stored value.
		Optional<std::string> text = "duckling";
		ASSERT_EQUAL("duckling", *text.filter([](const std::string& value) {
			return !value.empty();
		}));
		ASSERT_EQUAL("duckling", *text);
	}

	void testOrElse() {
		u64 calls = 0;
		int value = 7;
		// No arguments here either, so orElse and copyValueOrElse are used the same way.
		auto make = [&]() -> Optional<int> {
			calls++;
			return value;
		};

		Optional<int> opt = 1;
		ASSERT_EQUAL(1, *opt.orElse(make));
		ASSERT_EQUAL(u64{ 0 }, calls);

		Optional<int> empty;
		ASSERT_EQUAL(7, *empty.orElse(make));
		ASSERT_EQUAL(u64{ 1 }, calls);

		// The function is allowed to give back an empty optional as well.
		ASSERT_TRUE(empty.orElse([]() { return Optional<int>{}; }).empty());
	}

	void testInspect() {
		Optional<int> opt  = 1;
		int           seen = 0;
		opt.inspect([&](int value) { seen = value; });
		ASSERT_EQUAL(1, seen);

		// The reference is not const, so the stored value can be edited in place.
		opt.inspect([](int& value) { value++; });
		ASSERT_EQUAL(2, *opt);

		// inspect hands the optional back, so it fits in the middle of a chain.
		auto chained = opt.inspect([&](int value) { seen = value; }
		).map([](int value) { return value * 2; });
		ASSERT_EQUAL(4, *chained);
		ASSERT_EQUAL(2, seen);

		seen = 0;
		Optional<int> empty;
		empty.inspect([&](int value) { seen = value; });
		ASSERT_EQUAL(0, seen);
	}

	void testFlatten() {
		Optional<Optional<int>> nested = Optional<int>(1);
		ASSERT_EQUAL(1, *nested.flatten());

		Optional<Optional<int>> inner_empty = Optional<int>{};
		ASSERT_TRUE(inner_empty.flatten().empty());

		Optional<Optional<int>> outer_empty;
		ASSERT_TRUE(outer_empty.flatten().empty());
	}

	void testOkOr() {
		auto ok = Optional<int>(1).okOr<std::string>("empty");
		match_optional(ok) {
			opt_some(value) ASSERT_EQUAL(1, value);
			opt_err(error [[maybe_unused]]) fail("okOr made an error out of a stored value");
		}

		auto error = Optional<int>().okOr<std::string>("empty");
		match_optional(error) {
			opt_some(value [[maybe_unused]]) fail("okOr made a value out of an empty optional");
			opt_err(err) ASSERT_EQUAL("empty", err);
		}
	}

	void testTakeAndReplace() {
		Optional<std::string> opt   = "hello";
		auto                  taken = opt.take();
		ASSERT_TRUE(opt.empty());
		ASSERT_EQUAL("hello", *taken);

		Optional<std::string> nothing;
		ASSERT_TRUE(nothing.take().empty());

		Optional<int> number = 1;
		ASSERT_EQUAL(1, *number.replace(2));
		ASSERT_EQUAL(2, *number);

		Optional<int> empty;
		ASSERT_TRUE(empty.replace(5).empty());
		ASSERT_EQUAL(5, *empty);
	}

	void testGetOrInsert() {
		Optional<std::string> opt;
		std::string&          inserted = opt.getOrInsert("abc");
		ASSERT_EQUAL("abc", inserted);

		// The reference points at the stored value, not at a copy.
		inserted.push_back('d');
		ASSERT_EQUAL("abcd", *opt);

		// A stored value is never overwritten.
		ASSERT_EQUAL("abcd", opt.getOrInsert("zzz"));
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
