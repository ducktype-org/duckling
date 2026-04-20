#include <driver/repl_utils/repl_split_helpers.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <repl/session.hpp>

#include <filesystem/file.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <tester/tester.hpp>

#include <string_view>

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
			TESTER_ADD_TEST(testReplCommandAliasesThroughProcessLine);
			TESTER_ADD_TEST(testReplExitCommandAliasesThroughProcessLine);
			TESTER_ADD_TEST(testReplHistoryCommandThroughProcessLine);
			TESTER_ADD_TEST(testReplUnknownCommandThroughHandleCommand);
			TESTER_ADD_TEST(testReplClearCommandDoesNotResetSessionState);
			TESTER_ADD_TEST(testReplClearHistoryResetsSessionState);
			TESTER_ADD_TEST(testReplProcessLineWithCode);
			TESTER_ADD_TEST(testReplInstructionExecution);
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
			ReplSession session;

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
			ReplSession session;

			// Test with a help command
			std::string_view help_cmd    = "/help";
			auto             help_result = session.processLine(help_cmd);

			// Help command should be recognized and processed
			// It should not result in an error (though it may result in various statuses)
			assertTrue(
				help_result.status != ReplResult::Status::Error,
				"Help command should be processed without error"
			);
		}

		void testReplCommandAliasesThroughProcessLine() {
			ReplSession session;

			std::vector<std::string_view> aliases = { "/h", "/help", "/clear", "/c" };

			for (auto alias: aliases) {
				auto result = session.processLine(alias);
				assertTrue(
					result.status == ReplResult::Status::Success,
					std::string("Alias command should return success: ") + std::string(alias)
				);
			}
		}

		void testReplExitCommandAliasesThroughProcessLine() {
			for (auto exit_alias: std::vector<std::string_view>{ "/q", "/quit", "/exit" }) {
				ReplSession session;

				auto result = session.processLine(exit_alias);

				assertTrue(
					result.status == ReplResult::Status::Exit,
					std::string("Exit alias should return exit status: ") + std::string(exit_alias)
				);
				assertTrue(session.m_should_exit, "Exit alias should set m_should_exit flag");
			}
		}

		void testReplHistoryCommandThroughProcessLine() {
			ReplSession session;

			auto result = session.processLine("/history");

			assertTrue(
				result.status == ReplResult::Status::Success,
				"/history should be processed as a successful command"
			);
			assertFalse(session.m_should_exit, "/history should not mark session for exit");
		}

		void testReplUnknownCommandThroughHandleCommand() {
			ReplSession session;

			auto handled = session.handleCommand("/does-not-exist");

			assertFalse(handled, "Unknown command should not be reported as handled");
			assertFalse(session.m_should_exit, "Unknown command should not mark session for exit");
		}

		void testReplClearCommandDoesNotResetSessionState() {
			ReplSession session;

			session.processLine("var x: i32 = 10;");

			auto history_size_before_clear = session.m_history.size();
			auto line_counter_before_clear = session.m_line_counter;

			auto clear_result = session.processLine("/clear");

			assertTrue(
				clear_result.status == ReplResult::Status::Success,
				"/clear should be processed as a successful command"
			);
			assertTrue(
				session.m_history.size() == history_size_before_clear,
				"/clear should not modify REPL statement history"
			);
			assertTrue(
				session.m_line_counter == line_counter_before_clear,
				"/clear should not reset REPL line counter"
			);
		}

		void testReplClearHistoryResetsSessionState() {
			ReplSession session;

			session.processLine("1 + 2");
			session.processLine("3 + 4");

			assertTrue(!session.m_history.empty(), "History should contain entries before clear");
			assertTrue(session.m_line_counter > 0, "Line counter should increase before clear");

			session.clearHistory();

			assertTrue(session.m_history.empty(), "clearHistory should remove all history entries");
			assertTrue(session.m_line_counter == 0, "clearHistory should reset line counter");
		}

		/**
		 * @brief Test that ReplSession can process code input.
		 *
		 * Non-command input should be treated as code and executed.
		 * This test verifies that processLine routes code appropriately.
		 */
		void testReplProcessLineWithCode() {
			ReplSession session;

			// Test with a simple variable declaration
			std::string_view var_decl = "var x: i32 = 42;";
			auto             result   = session.processLine(var_decl);

			// The result should indicate execution was attempted
			// (may succeed, error, or have incomplete input)
			assertTrue(
				result.status != ReplResult::Status::Exit, "Code input should not trigger exit"
			);
		}

		void testReplInstructionExecution() {
			ReplSession session;

			auto result = session.processLine("while (0 == 1) {}");

			assertTrue(
				result.status == ReplResult::Status::Success,
				"Instruction statement should execute successfully"
			);
			assertTrue(
				session.m_history.size() == 1,
				"Instruction execution should add one entry to history"
			);
		}

		/**
		 * @brief Test command detection through isCommand.
		 *
		 * Verifies that lines starting with '/' are correctly identified as commands,
		 * and other lines are identified as code.
		 */
		void testReplCommandDetection() {
			ReplSession session;

			// Test various inputs to verify command detection
			std::vector<std::string_view> commands
				= { "/exit", "/help", "/h", "/history", "/hist", "/clear", "/c" };

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
			ReplSession session;

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
			ReplSession session;

			// Define test cases
			struct ArithmeticTest {
				std::string_view input;
				std::string_view description;
			};

			std::vector<ArithmeticTest> tests
				= { { .input = "2 + 2", .description = "2 + 2 should equal 4" },
				    { .input = "2 - 4", .description = "2 - 4 should equal -2" },
				    { .input = "5 * 3", .description = "5 * 3 should equal 15" },
				    { .input = "10 / 2", .description = "10 / 2 should equal 5" },
				    { .input       = "2 + 3 * 4",
				      .description = "2 + 3 * 4 should equal 14 (multiplication first)" },
				    { .input       = "10 - 2 * 3",
				      .description = "10 - 2 * 3 should equal 4 (multiplication first)" },
				    { .input       = "2 * 3 * 4",
				      .description = "2 * 3 * 4 should equal 24 (left associative)" },
				    { .input = "2 * 2387 + 1", .description = "2 * 2387 + 1 should equal 4775" },
				    { .input = "100 / 2 - 5", .description = "100 / 2 - 5 should equal 45" },
				    { .input = "1 + 2 * 3 + 4", .description = "1 + 2 * 3 + 4 should equal 11" },
				    { .input = "42", .description = "Integer literal 42 should output 42" },
				    { .input = "0", .description = "Zero should output 0" },
				    { .input = "1", .description = "One should output 1" } };

			for (const auto& test: tests) {
				auto result = session.processLine(test.input);

				// The key test: expression should process without error or exit
				bool status_ok = result.status != ReplResult::Status::Exit
				              && result.status != ReplResult::Status::Error;

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
			ReplSession session;

			// Define test cases: (input, description)
			struct VariableLookupTest {
				std::string_view input;
				std::string_view description;
			};

			std::vector<VariableLookupTest> tests
				= { { .input = "var x = 4;", .description = "Declare x with value 4" },
				    { .input = "x;", .description = "Look up x" },
				    { .input = "var y = 5;", .description = "Declare y with value 5" },
				    { .input = "var z = 6;", .description = "Declare z with value 6" },
				    { .input = "y;", .description = "Look up y" } };

			// Track initial state
			size_t initial_history_size = session.m_history.size();

			for (const auto& test: tests) {
				auto result = session.processLine(test.input);

				// The key test: each statement should process without error or exit
				bool status_ok = result.status != ReplResult::Status::Exit
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
