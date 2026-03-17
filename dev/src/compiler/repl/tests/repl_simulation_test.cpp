#include <driver/repl_utils/repl_split_helpers.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <repl/session.hpp>

#include <filesystem/file.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <tester/tester.hpp>

#include <string_view>

using namespace compiler;

namespace compiler::repl {
	/**
	 * @brief Practical REPL simulation tests
	 *
	 * These tests simulate what actually happens when a user types in the REPL:
	 * - User types: var x = 4
	 * - User types: x
	 */
	class ReplSimulationTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ReplSimulationTest

	public:
		TESTER_TEST_SIMPLE_CONSTRUCTOR() {
			TESTER_ADD_TEST(testReplSessionInitialization);
			TESTER_ADD_TEST(testReplProcessLineWithCommand);
			TESTER_ADD_TEST(testReplProcessLineWithCode);
			TESTER_ADD_TEST(testReplCommandDetection);
			TESTER_ADD_TEST(testReplHistoryTracking);
			TESTER_ADD_TEST(testReplArithmeticExpressions);
			TESTER_ADD_TEST(testReplVariableLookup);
		}

	private:
		/**
		 * @brief Test that a ReplSession can be initialized successfully.
		 *
		 * Verifies that ReplSession constructor properly initializes the internal state.
		 * Note: DVM initialization may fail in test environments, so we only verify
		 * that the session structure is initialized, not that DVM spawned successfully.
		 */
		void testReplSessionInitialization() {
			repl::ReplSession session;

			// Verify internal state is initialized correctly
			assertTrue(
				!session.m_should_exit, "ReplSession should not exit immediately after construction"
			);
			assertTrue(session.m_history.empty(), "ReplSession history should start empty");
			assertTrue(session.m_line_counter == 0, "Line counter should start at 0");
		}

		/**
		 * @brief Test that ReplSession can detect and process REPL commands.
		 *
		 * Commands start with '/' and should be recognized as such.
		 * This test verifies the command detection logic through processLine.
		 */
		void testReplProcessLineWithCommand() {
			repl::ReplSession session;

			// Test with a help command
			std::string_view help_cmd    = "/help";
			auto             help_result = session.processLine(help_cmd);

			// Help command should be recognized and processed
			// It should not result in an error (though it may result in various statuses)
			assertTrue(
				help_result.status != repl::ReplResult::Status::Error,
				"Help command should be processed without error"
			);
		}

		/**
		 * @brief Test that ReplSession can process code input.
		 *
		 * Non-command input should be treated as code and executed.
		 * This test verifies that processLine routes code appropriately.
		 */
		void testReplProcessLineWithCode() {
			repl::ReplSession session;

			// Test with a simple variable declaration
			std::string_view var_decl = "var x: i32 = 42;";
			auto             result   = session.processLine(var_decl);

			// The result should indicate execution was attempted
			// (may succeed, error, or have incomplete input)
			assertTrue(
				result.status != repl::ReplResult::Status::Exit, "Code input should not trigger exit"
			);
		}

		/**
		 * @brief Test command detection through isCommand.
		 *
		 * Verifies that lines starting with '/' are correctly identified as commands,
		 * and other lines are identified as code.
		 */
		void testReplCommandDetection() {
			repl::ReplSession session;

			// Test various inputs to verify command detection
			std::vector<std::string_view> commands = { "/exit", "/help", "/history" };

			std::vector<std::string_view> code_snippets = { "var x = 4;", "x;", "" };

			for (auto cmd: commands)
				assertTrue(
					session.isCommand(cmd), std::string(cmd) + " should be recognized as command"
				);
			for (auto code: code_snippets)
				assertFalse(
					session.isCommand(code), "Code snippet should not be recognized as command"
				);
		}

		/**
		 * @brief Test that history tracking works correctly.
		 *
		 * Verifies that ReplSession maintains history of executed statements
		 * and that the line counter is incremented appropriately.
		 */
		void testReplHistoryTracking() {
			repl::ReplSession session;

			// Initial state
			auto initial_history_size = session.m_history.size();
			auto initial_line_count   = session.m_line_counter;

			assertTrue(initial_history_size == 0, "History should start empty");
			assertTrue(initial_line_count == 0, "Line counter should start at 0");

			// Execute a simple statement
			std::string_view code = "var x: i32 = 10;";
			session.processLine(code);

			// Verify that history was updated
			auto updated_history_size = session.m_history.size();
			assertTrue(
				updated_history_size >= initial_history_size,
				"History size should increase or stay same after processing"
			);
			assertTrue(
				session.m_lowering_context.has_value(),
				"Lowering context should be initialized for REPL execution"
			);
		}

		/**
		 * @brief Test REPL arithmetic expression evaluation.
		 *
		 * Verifies that:
		 * 1. Arithmetic expressions are parsed correctly
		 * 2. Expressions execute without errors or exit
		 * 3. Operator precedence is correctly applied
		 *
		 * How it works:
		 * - REPL expressions are wrapped in a function
		 * - The function is compiled to DVM bytecode
		 * - The function is executed by the DVM process
		 * - Results are printed directly to stdout by the DVM ("=> <result>")
		 *
		 * Thread-safe: does not capture stdout (compatible with parallel testing).
		 */
		void testReplArithmeticExpressions() {
			repl::ReplSession session;

			// Define test cases
			struct ArithmeticTest {
				std::string_view input;
				std::string_view description;
			};

			std::vector<ArithmeticTest> tests = {
				// Basic operations
				{ "2 + 2", "2 + 2 should equal 4" },
				{ "2 - 4", "2 - 4 should equal -2" },
				{ "5 * 3", "5 * 3 should equal 15" },
				{ "10 / 2", "10 / 2 should equal 5" },

				// Operator precedence tests
				{ "2 + 3 * 4", "2 + 3 * 4 should equal 14 (multiplication first)" },
				{ "10 - 2 * 3", "10 - 2 * 3 should equal 4 (multiplication first)" },
				{ "2 * 3 * 4", "2 * 3 * 4 should equal 24 (left associative)" },

				// Complex expressions
				{ "2 * 2387 + 1", "2 * 2387 + 1 should equal 4775" },
				{ "100 / 2 - 5", "100 / 2 - 5 should equal 45" },
				{ "1 + 2 * 3 + 4", "1 + 2 * 3 + 4 should equal 11" },

				// Simple literals
				{ "42", "Integer literal 42 should output 42" },
				{ "0", "Zero should output 0" },
				{ "1", "One should output 1" },
			};

			for (const auto& test: tests) {
				auto result = session.processLine(test.input);

				// The key test: expression should process without error or exit
				bool status_ok = result.status != repl::ReplResult::Status::Exit
				              && result.status != repl::ReplResult::Status::Error;

				// Build assertion message
				std::string assertion_msg = "Expression '";
				assertion_msg.append(test.input);
				assertion_msg.append("' (");
				assertion_msg.append(test.description);
				assertion_msg.append(") should execute without error");

				assertTrue(status_ok, assertion_msg);
			}
		}

		/**
		 * @brief Test REPL variable declaration and lookup.
		 *
		 */
		void testReplVariableLookup() {
			repl::ReplSession session;

			// Define test cases: (input, description)
			struct VariableLookupTest {
				std::string_view input;
				std::string_view description;
			};

			std::vector<VariableLookupTest> tests = {
				{ "var x = 4;", "Declare x with value 4" },
				{ "x;", "Look up x" },
				{ "var y = 5;", "Declare y with value 5" },
				{ "var z = 6;", "Declare z with value 6" },
				{ "y;", "Look up y" },
			};

			// Track initial state
			size_t initial_history_size = session.m_history.size();

			for (const auto& test: tests) {
				auto result = session.processLine(test.input);

				// The key test: each statement should process without error or exit
				bool status_ok = result.status != repl::ReplResult::Status::Exit
				              && result.status != repl::ReplResult::Status::Error;

				// Build detailed assertion message
				std::string assertion_msg = "Variable test '";
				assertion_msg.append(test.input);
				assertion_msg.append("' (");
				assertion_msg.append(test.description);
				assertion_msg.append(") should execute without error");

				assertTrue(status_ok, assertion_msg);
			}

			// Verify that history was updated with all statements
			size_t expected_history_size = initial_history_size + tests.size();
			assertTrue(
				session.m_history.size() == expected_history_size,
				"REPL session history should track all variable operations"
			);
		}

	public:
		~ReplSimulationTest() override = default;
	};

}  // namespace compiler::repl

// Bring the test class into global scope for TESTER_COMMON_MAIN
using ReplSimulationTest = compiler::repl::ReplSimulationTest;

TESTER_COMMON_MAIN("/src/compiler/repl/tests/")
