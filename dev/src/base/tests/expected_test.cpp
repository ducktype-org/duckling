/**
 * @file expected_test.cpp
 * @brief Tests for the match_expected macros.
 */

#include <base/collections/expected.hpp>
#include <base/types/ints.hpp>

#include <tester/tester.hpp>

#include <expected>
#include <string>
#include <utility>

class ExpectedTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ExpectedTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(matchTest);
		TESTER_ADD_TEST(voidTest);
		TESTER_ADD_TEST(moveTest);
		TESTER_ADD_TEST(shorthandTest);
		TESTER_ADD_TEST(mutationTest);
		TESTER_ADD_TEST(singleEvaluationTest);
		TESTER_ADD_TEST(testFromDocs);
	}

private:
	/// A type that can only be moved, so a copy made by a macro would not compile.
	struct MoveOnly {
		std::string val;

		explicit MoveOnly(std::string val): val(std::move(val)) {}

		MoveOnly(MoveOnly&&)            = default;
		MoveOnly& operator=(MoveOnly&&) = default;

		MoveOnly(const MoveOnly&)            = delete;
		MoveOnly& operator=(const MoveOnly&) = delete;
	};

	void matchTest() {
		std::expected<int, std::string> expected = 4;
		match_expected(expected) {
			exp_ok(value) ASSERT_EQUAL(4, value);
			exp_err(error [[maybe_unused]]) fail("A stored value was matched as an error");
		}

		expected = std::unexpected<std::string>("quack");
		match_expected(expected) {
			exp_ok(value [[maybe_unused]]) fail("An error was matched as a value");
			exp_err(error) ASSERT_EQUAL("quack", error);
		}

		// Neither branch has to name what it matched.
		bool entered = false;
		match_expected(expected) {
			exp_ok() fail("An error was matched as a value");
			exp_err() entered = true;
		}
		ASSERT_TRUE(entered);
	}

	void voidTest() {
		bool entered = false;

		std::expected<void, std::string> nothing;
		match_expected(nothing) {
			exp_ok() entered = true;
			exp_err(error [[maybe_unused]]) fail("An empty expected is not an error");
		}
		ASSERT_TRUE(entered);

		nothing = std::unexpected<std::string>("disk is full");
		match_expected(nothing) {
			exp_ok() fail("An error was matched as a value");
			exp_err(error) ASSERT_EQUAL("disk is full", error);
		}
	}

	void moveTest() {
		std::expected<MoveOnly, MoveOnly> expected(std::in_place, "value");
		match_expected(expected) {
			exp_ok_move(value) {
				MoveOnly taken = std::move(value);
				ASSERT_EQUAL("value", taken.val);
			}
			exp_err_move(error [[maybe_unused]]) fail("A stored value was matched as an error");
		}

		expected = std::unexpected(MoveOnly("error"));
		match_expected(expected) {
			exp_ok_move(value [[maybe_unused]]) fail("An error was matched as a value");
			exp_err_move(error) {
				MoveOnly taken = std::move(error);
				ASSERT_EQUAL("error", taken.val);
			}
		}
	}

	void shorthandTest() {
		std::expected<int, std::string> expected = 4;

		bool entered = false;
		if_exp_ok(expected, value) {
			ASSERT_EQUAL(4, value);
			entered = true;
		}
		ASSERT_TRUE(entered);

		if_exp_err(expected, error [[maybe_unused]]) fail("A stored value was matched as an error");

		expected = std::unexpected<std::string>("quack");

		entered = false;
		if_exp_err(expected, error) {
			ASSERT_EQUAL("quack", error);
			entered = true;
		}
		ASSERT_TRUE(entered);

		if_exp_ok(expected, value [[maybe_unused]]) fail("An error was matched as a value");

		// The name may be left out here as well.
		entered = false;
		if_exp_err(expected) { entered = true; }
		ASSERT_TRUE(entered);

		if_exp_ok(expected) fail("An error was matched as a value");
	}

	void mutationTest() {
		// The matched value is bound by reference, so writing to it writes into the expected.
		std::expected<int, std::string> expected = 4;
		match_expected(expected) {
			exp_ok(value) value = 5;
			exp_err() fail("A stored value was matched as an error");
		}
		ASSERT_EQUAL(5, expected.value());

		if_exp_ok(expected, value) value = 6;
		ASSERT_EQUAL(6, expected.value());
	}

	void singleEvaluationTest() {
		// The matched expression is evaluated exactly once, even by the shorthands.
		u64  calls     = 0;
		auto make_call = [&calls]() -> std::expected<int, std::string> {
			calls++;
			return 4;
		};

		match_expected(make_call()) {
			exp_ok(value) ASSERT_EQUAL(4, value);
			exp_err() fail("A stored value was matched as an error");
		}
		ASSERT_EQUAL(1, calls);

		if_exp_ok(make_call(), value) ASSERT_EQUAL(4, value);
		ASSERT_EQUAL(2, calls);

		if_exp_err(make_call()) fail("A stored value was matched as an error");
		ASSERT_EQUAL(3, calls);
	}

	void testFromDocs() {
		std::expected<int, std::string> result = 4;

		std::string out;
		match_expected(result) {
			exp_ok(value) { out += "Value: " + std::to_string(value) + "\n"; }
			exp_err(error) { out += "Error: " + error + "\n"; }
		}

		std::expected<void, std::string> saved;
		match_expected(saved) {
			exp_ok() { out += "Saved!\n"; }
			exp_err(error) { out += "Error: " + error + "\n"; }
		}

		if_exp_ok(result, value) { out += "Value: " + std::to_string(value) + "\n"; }
		if_exp_err(result, error) { out += "Error: " + error + "\n"; }

		ASSERT_EQUAL("Value: 4\nSaved!\nValue: 4\n", out);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
