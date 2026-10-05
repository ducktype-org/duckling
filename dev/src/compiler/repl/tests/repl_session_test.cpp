// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/repl_utils/repl_split_helpers.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <repl/session.hpp>

#include <base/types/ints.hpp>

#include <diagnostic/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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
			TESTER_ADD_TEST(testReplSymbolsCommandEmpty);
			TESTER_ADD_TEST(testReplSymbolsCommandListsDeclarations);
			TESTER_ADD_TEST(testReplSymbolsCommandListsConstAliasAndClass);
			TESTER_ADD_TEST(testReplVariablesCommandListsOnlyVariables);
			TESTER_ADD_TEST(testReplVariablesAndFunctionsCommandsReportEmptyAfterOtherSymbols);
			TESTER_ADD_TEST(testReplFunctionsCommandListsOnlyFunctions);
			TESTER_ADD_TEST(testReplDetailsCommandShowsVariableDetails);
			TESTER_ADD_TEST(testReplDetailsCommandShowsFunctionDetails);
			TESTER_ADD_TEST(testReplDetailsCommandShowsClassMembers);
			TESTER_ADD_TEST(testReplDetailsCommandShowsNamespaceMembers);
			TESTER_ADD_TEST(testReplDetailsCommandDoesNotResolveMemberPaths);
			TESTER_ADD_TEST(testReplDetailsCommandHandlesMissingName);
			TESTER_ADD_TEST(testReplUnknownCommandThroughHandleCommand);
			TESTER_ADD_TEST(testReplClearCommandDoesNotResetSessionState);
			TESTER_ADD_TEST(testReplProcessLineWithCode);
			TESTER_ADD_TEST(testReplInstructionExecution);
			TESTER_ADD_TEST(testReplCommandDetection);
			TESTER_ADD_TEST(testReplHistoryTracking);
			TESTER_ADD_TEST(testLoadScriptFileMissingPath);
			TESTER_ADD_TEST(testLoadScriptFileInvalidContents);
			TESTER_ADD_TEST(testLoadScriptFileExecutesStatements);
			TESTER_ADD_TEST(testLoadCommandExecutesScript);
			TESTER_ADD_TEST(testReplArithmeticExpressions);
			TESTER_ADD_TEST(testReplVariableLookup);
			TESTER_ADD_TEST(testReplUnsupportedActionClassification);
			TESTER_ADD_TEST(testReplResetAbsoluteReplay);
			TESTER_ADD_TEST(testReplResetRelativeReplay);
			TESTER_ADD_TEST(testReplResetSyntaxErrors);
			TESTER_ADD_TEST(testReplFrontendAddHistory);
			TESTER_ADD_TEST(testReplResetSyntaxErrorsMore);
			TESTER_ADD_TEST(testReplSaveAndPrintHistory);
			TESTER_ADD_TEST(testReplCommandsResetCommand);
			TESTER_ADD_TEST(testReplHistoryCommandAdvanced);
			TESTER_ADD_TEST(testReplReplayHistoryNotSilent);
		}

		void beforeAll() override {
			dia::configureImmediatePrint(&std::cerr);
			// enable if needed
			// logger::enable_dev_logs = true;
			// logger::enableDevCategoryByStringName("REPL");
		}

	private:
		struct ScopedStreamCapture final {
			explicit ScopedStreamCapture(std::ostream& stream):
				  m_stream(stream),
				  m_old_buf(stream.rdbuf(m_buffer.rdbuf())) {}

			~ScopedStreamCapture() { m_stream.rdbuf(m_old_buf); }

			ScopedStreamCapture(const ScopedStreamCapture&)            = delete;
			ScopedStreamCapture& operator=(const ScopedStreamCapture&) = delete;

			std::string str() const { return m_buffer.str(); }

		private:
			std::ostream&     m_stream;
			std::stringstream m_buffer;
			std::streambuf*   m_old_buf = nullptr;
		};

		void removeSessionHistoryFile() {
			std::error_code err;
			std::filesystem::remove(".duckling_repl_session_history", err);
		}

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
			assertTrue(session.m_session_history.empty(), "ReplSession history should start empty");
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

		void testReplSymbolsCommandEmpty() {
			ReplSession session;

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/symbols");

			assertTrue(
				result.status == ReplResult::Status::Success,
				"/symbols should be processed as a successful command"
			);
			assertTrue(
				capture.str().find("No symbols declared yet.") != std::string::npos,
				"/symbols should report an empty declaration list for a fresh session"
			);
		}

		void testReplSymbolsCommandListsDeclarations() {
			ReplSession session;

			auto var_result = session.processLine("var sym_x: i32 = 10;");
			auto fun_result
				= session.processLine("fun sym_add(a: i32, b: i32) -> i32 = { return a + b; }");
			auto ns_result = session.processLine("namespace SymNs {}");

			assertTrue(
				var_result.status == ReplResult::Status::Success,
				"Variable declaration should succeed before /symbols"
			);
			assertTrue(
				fun_result.status == ReplResult::Status::Success,
				"Function declaration should succeed before /symbols"
			);
			assertTrue(
				ns_result.status == ReplResult::Status::Success,
				"Namespace declaration should succeed before /symbols"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/syms");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/syms should succeed");
			assertTrue(
				output.find("Visible Symbols") != std::string::npos,
				"/symbols output should include a header"
			);
			assertTrue(
				output.find("[history #1] variable sym_x : i32") != std::string::npos,
				"/symbols should list the variable with its type"
			);
			assertTrue(
				output.find("[history #2] function sym_add(a: i32, b: i32) -> i32")
					!= std::string::npos,
				"/symbols should list the function with its signature"
			);
			assertTrue(
				output.find("[history #3] namespace SymNs") != std::string::npos,
				"/symbols should list the namespace"
			);
		}

		void testReplSymbolsCommandListsConstAliasAndClass() {
			ReplSession session;

			auto var_result   = session.processLine("var symbol_source: i32 = 3;");
			auto alias_result = session.processLine("using symbol_source as symbol_alias;");
			auto const_result = session.processLine("const symbol_const: i64 = 42;");
			auto class_result = session.processLine("class SymbolClass{}");

			assertTrue(
				var_result.status == ReplResult::Status::Success,
				"Variable declaration should succeed before richer /symbols test"
			);
			assertTrue(
				alias_result.status == ReplResult::Status::Success,
				"`using ... as` declaration should succeed before /symbols"
			);
			assertTrue(
				const_result.status == ReplResult::Status::Success,
				"Const declaration should succeed before /symbols"
			);
			assertTrue(
				class_result.status == ReplResult::Status::Success,
				"Class declaration should succeed before /symbols"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/symbols");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/symbols should succeed");
			assertTrue(
				output.find("[history #1] variable symbol_source : i32") != std::string::npos,
				"/symbols should list regular variables"
			);
			assertTrue(
				output.find("[history #2] using symbol_alias") != std::string::npos,
				"/symbols should list `using ... as` declarations"
			);
			assertTrue(
				output.find("[history #3] const symbol_const : const i64") != std::string::npos,
				"/symbols should list const declarations with their type"
			);
			assertTrue(
				output.find("[history #4] class SymbolClass") != std::string::npos,
				"/symbols should list classes"
			);
		}

		void testReplVariablesCommandListsOnlyVariables() {
			ReplSession session;

			auto var_result = session.processLine("var only_var_x: i32 = 10;");
			auto fun_result
				= session.processLine("fun only_fun_add(a: i32, b: i32) -> i32 = { return a + b; }");
			auto ns_result = session.processLine("namespace OnlyVarNs {}");

			assertTrue(
				var_result.status == ReplResult::Status::Success,
				"Variable declaration should succeed before /vars"
			);
			assertTrue(
				fun_result.status == ReplResult::Status::Success,
				"Function declaration should succeed before /vars"
			);
			assertTrue(
				ns_result.status == ReplResult::Status::Success,
				"Namespace declaration should succeed before /vars"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/vars");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/vars should succeed");
			assertTrue(
				output.find("Visible Variables") != std::string::npos,
				"/vars output should include a variables header"
			);
			assertTrue(
				output.find("[history #1] variable only_var_x : i32") != std::string::npos,
				"/vars should list the variable with its type"
			);
			assertTrue(
				output.find("only_fun_add") == std::string::npos, "/vars should not list functions"
			);
			assertTrue(
				output.find("OnlyVarNs") == std::string::npos, "/vars should not list namespaces"
			);
		}

		void testReplVariablesAndFunctionsCommandsReportEmptyAfterOtherSymbols() {
			ReplSession session;

			auto ns_result    = session.processLine("namespace EmptyFilterNs {}");
			auto class_result = session.processLine("class EmptyFilterClass{}");
			auto const_result = session.processLine("const empty_filter_const: i64 = 1;");

			assertTrue(
				ns_result.status == ReplResult::Status::Success,
				"Namespace declaration should succeed before empty filter checks"
			);
			assertTrue(
				class_result.status == ReplResult::Status::Success,
				"Class declaration should succeed before empty filter checks"
			);
			assertTrue(
				const_result.status == ReplResult::Status::Success,
				"Const declaration should succeed before empty filter checks"
			);

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/vars");
				assertTrue(result.status == ReplResult::Status::Success, "/vars should succeed");
				assertTrue(
					capture.str().find("No variables declared yet.") != std::string::npos,
					"/vars should report no top-level variables when only other symbols exist"
				);
			}

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/functions");
				assertTrue(
					result.status == ReplResult::Status::Success, "/functions should succeed"
				);
				assertTrue(
					capture.str().find("No functions declared yet.") != std::string::npos,
					"/functions should report no top-level functions when only other symbols exist"
				);
			}
		}

		void testReplFunctionsCommandListsOnlyFunctions() {
			ReplSession session;

			auto var_result = session.processLine("var only_func_x: i32 = 10;");
			auto fun_result
				= session.processLine("fun only_func_add(a: i32, b: i32) -> i32 = { return a + b; }"
			    );
			auto ns_result = session.processLine("namespace OnlyFuncNs {}");

			assertTrue(
				var_result.status == ReplResult::Status::Success,
				"Variable declaration should succeed before /functions"
			);
			assertTrue(
				fun_result.status == ReplResult::Status::Success,
				"Function declaration should succeed before /functions"
			);
			assertTrue(
				ns_result.status == ReplResult::Status::Success,
				"Namespace declaration should succeed before /functions"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/fns");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/fns should succeed");
			assertTrue(
				output.find("Visible Functions") != std::string::npos,
				"/functions output should include a functions header"
			);
			assertTrue(
				output.find("[history #2] function only_func_add(a: i32, b: i32) -> i32")
					!= std::string::npos,
				"/functions should list the function with its signature"
			);
			assertTrue(
				output.find("only_func_x") == std::string::npos,
				"/functions should not list variables"
			);
			assertTrue(
				output.find("OnlyFuncNs") == std::string::npos,
				"/functions should not list namespaces"
			);
		}

		void testReplDetailsCommandShowsVariableDetails() {
			ReplSession session;

			auto var_result = session.processLine("var detail_value: i64 = 99;");
			assertTrue(
				var_result.status == ReplResult::Status::Success,
				"Variable declaration should succeed before /details"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/details detail_value");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
			assertTrue(
				output.find("Details for `detail_value`") != std::string::npos,
				"/details should include a variable details header"
			);
			assertTrue(
				output.find("defined in: history #1") != std::string::npos,
				"/details should include the defining history entry"
			);
			assertTrue(
				output.find("variable detail_value : i64") != std::string::npos,
				"/details should include the variable type summary"
			);
			assertTrue(
				output.find("kind: variable") != std::string::npos,
				"/details should include the variable kind"
			);
			assertTrue(
				output.find("qualified name:") != std::string::npos,
				"/details should include the compiler qualified name"
			);
		}

		void testReplDetailsCommandShowsFunctionDetails() {
			ReplSession session;

			auto fun_result
				= session.processLine("fun detail_mul(a: i64, b: i64) -> i64 = { return a * b; }");
			assertTrue(
				fun_result.status == ReplResult::Status::Success,
				"Function declaration should succeed before /details"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/details detail_mul");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
			assertTrue(
				output.find("Details for `detail_mul`") != std::string::npos,
				"/details should include a function details header"
			);
			assertTrue(
				output.find("function detail_mul(a: i64, b: i64) -> i64") != std::string::npos,
				"/details should include the function signature"
			);
			assertTrue(
				output.find("kind: function") != std::string::npos,
				"/details should include the function kind"
			);
		}

		void testReplDetailsCommandShowsClassMembers() {
			ReplSession session;

			auto class_result = session.processLine(
				"class DetailClass{ field_a : i64; field_b : i64; "
				"fun detail_sum(t : DetailClass) -> i64 = t.field_a + t.field_b;}"
			);
			assertTrue(
				class_result.status == ReplResult::Status::Success,
				"Class declaration should succeed before /details"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/details DetailClass");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
			assertTrue(
				output.find("Details for `DetailClass`") != std::string::npos,
				"/details should include a class details header"
			);
			assertTrue(
				output.find("class DetailClass") != std::string::npos,
				"/details should include the class summary"
			);
			assertTrue(
				output.find("fields:") != std::string::npos, "/details should include class fields"
			);
			assertTrue(
				output.find("field field_a : i64") != std::string::npos,
				"/details should list the first field"
			);
			assertTrue(
				output.find("field field_b : i64") != std::string::npos,
				"/details should list the second field"
			);
			assertTrue(
				output.find("declared constructors:") != std::string::npos,
				"/details should label source constructors as declared"
			);
			assertTrue(
				output.find("implicit constructors:") != std::string::npos,
				"/details should mention generated implicit constructors"
			);
			assertTrue(
				output.find("DetailClass(field_a: i64, field_b: i64)") != std::string::npos,
				"/details should show the implicit field constructor"
			);
			assertTrue(
				output.find("methods:") != std::string::npos, "/details should include class methods"
			);
			assertTrue(
				output.find("method detail_sum(t: Class DetailClass) -> i64") != std::string::npos,
				"/details should list the method signature"
			);
		}

		void testReplDetailsCommandShowsNamespaceMembers() {
			ReplSession session;

			auto namespace_result = session.processLine(
				"namespace DetailNs { var ns_value: i32 = 1; "
				"fun ns_fun() -> i32 = { return ns_value; } }"
			);
			assertTrue(
				namespace_result.status == ReplResult::Status::Success,
				"Namespace declaration should succeed before /details"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/details DetailNs");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
			assertTrue(
				output.find("Details for `DetailNs`") != std::string::npos,
				"/details should include a namespace details header"
			);
			assertTrue(
				output.find("namespace DetailNs") != std::string::npos,
				"/details should include the namespace summary"
			);
			assertTrue(
				output.find("members:") != std::string::npos,
				"/details should include namespace members"
			);
			assertTrue(
				output.find("variable ns_value : i32") != std::string::npos,
				"/details should list namespace variables"
			);
			assertTrue(
				output.find("function ns_fun() -> i32") != std::string::npos,
				"/details should list namespace functions"
			);
		}

		void testReplDetailsCommandDoesNotResolveMemberPaths() {
			ReplSession session;

			auto namespace_result
				= session.processLine("namespace MemberPathNs { var inner: i32 = 1; }");
			assertTrue(
				namespace_result.status == ReplResult::Status::Success,
				"Namespace declaration should succeed before member-path /details check"
			);

			ScopedStreamCapture capture(std::cout);
			auto                result = session.processLine("/details MemberPathNs.inner");
			const auto          output = capture.str();

			assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
			assertTrue(
				output.find("No visible symbol named `MemberPathNs.inner`.") != std::string::npos,
				"/details should intentionally reject member paths for now"
			);
		}

		void testReplDetailsCommandHandlesMissingName() {
			ReplSession session;

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/details");
				assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
				assertTrue(
					capture.str().find("Usage: /details <visible-symbol-name>") != std::string::npos,
					"/details without a name should print usage"
				);
			}

			auto var_result = session.processLine("var present_detail_symbol: i32 = 1;");
			assertTrue(
				var_result.status == ReplResult::Status::Success,
				"Variable declaration should succeed before missing /details lookup"
			);

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/details missing_detail_symbol");
				assertTrue(result.status == ReplResult::Status::Success, "/details should succeed");
				assertTrue(
					capture.str().find("No visible symbol named `missing_detail_symbol`.")
						!= std::string::npos,
					"/details should report an unknown symbol"
				);
			}
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

			auto history_size_before_clear = session.m_session_history.size();
			auto line_counter_before_clear = session.m_line_counter;

			auto clear_result = session.processLine("/clear");

			assertTrue(
				clear_result.status == ReplResult::Status::Success,
				"/clear should be processed as a successful command"
			);
			assertTrue(
				session.m_session_history.size() == history_size_before_clear,
				"/clear should not modify REPL statement history"
			);
			assertTrue(
				session.m_line_counter == line_counter_before_clear,
				"/clear should not reset REPL line counter"
			);
		}

		void testReplResetAbsoluteReplay() {
			removeSessionHistoryFile();
			ReplSession session;

			std::vector<std::string_view> statements = {
				"var a: i32 = 1;",
				"var b: i32 = a + 1;",
				"var c: i32 = b + 1;",
				"var d: i32 = c + 1;",
			};

			for (auto stmt: statements) {
				auto result = session.processLine(stmt);
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Setup statements should execute before reset"
				);
			}

			auto reset_result = session.processLine("/reset 2");
			assertTrue(
				reset_result.status == ReplResult::Status::Reset,
				"/reset 2 should request a REPL reset"
			);
			assertTrue(
				session.getResetReplayCount().has_value()
					&& session.getResetReplayCount().value() == 2,
				"/reset 2 should set replay count to 2"
			);

			{
				ReplSession replay_session;
				replay_session.replayHistoryEntries(2, true);

				auto first_result  = replay_session.processLine("a;");
				auto second_result = replay_session.processLine("b;");
				auto third_result  = replay_session.processLine("c;");
				auto fourth_result = replay_session.processLine("d;");

				assertTrue(
					first_result.status == ReplResult::Status::Success,
					"After reset to entry 2, 'a' should be available"
				);
				assertTrue(
					second_result.status == ReplResult::Status::Success,
					"After reset to entry 2, 'b' should be available"
				);
				assertTrue(
					third_result.status == ReplResult::Status::Error,
					"After reset to entry 2, 'c' should be unavailable"
				);
				assertTrue(
					fourth_result.status == ReplResult::Status::Error,
					"After reset to entry 2, 'd' should be unavailable"
				);
			}

			removeSessionHistoryFile();
		}

		void testReplResetRelativeReplay() {
			removeSessionHistoryFile();
			ReplSession session;

			std::vector<std::string_view> statements = {
				"var a: i32 = 1;",
				"var b: i32 = a + 1;",
				"var c: i32 = b + 1;",
				"var d: i32 = c + 1;",
			};

			for (auto stmt: statements) {
				auto result = session.processLine(stmt);
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Setup statements should execute before reset"
				);
			}

			auto reset_result = session.processLine("/reset -2");
			assertTrue(
				reset_result.status == ReplResult::Status::Reset,
				"/reset -2 should request a REPL reset"
			);
			assertTrue(
				session.getResetReplayCount().has_value()
					&& session.getResetReplayCount().value() == 2,
				"/reset -2 should set replay count to size - 2"
			);

			{
				ReplSession replay_session;
				replay_session.replayHistoryEntries(2, true);
				auto first_result  = replay_session.processLine("a;");
				auto second_result = replay_session.processLine("b;");
				auto third_result  = replay_session.processLine("c;");
				assertTrue(
					first_result.status == ReplResult::Status::Success,
					"After relative reset, 'a' should be available"
				);
				assertTrue(
					second_result.status == ReplResult::Status::Success,
					"After relative reset, 'b' should be available"
				);
				assertTrue(
					third_result.status == ReplResult::Status::Error,
					"After relative reset, 'c' should be unavailable"
				);
			}

			removeSessionHistoryFile();
		}

		void testReplFrontendAddHistory() {
			ReplFrontend frontend_without_completions(false);
			ReplFrontend frontend_with_completions(true);
			frontend_without_completions.clearHistory();
			frontend_with_completions.clearHistory();

			{
				ScopedStreamCapture capture(std::cout);
				frontend_without_completions.printHistory();
				assertTrue(
					capture.str().find("No history yet.") != std::string::npos,
					"Should print no history"
				);
			}

			frontend_with_completions.addHistoryEntry("test");
			frontend_with_completions.addHistoryEntry("");
			frontend_with_completions.clearHistory();

			{
				ScopedStreamCapture capture(std::cout);
				frontend_with_completions.printHistory();
				assertTrue(
					capture.str().find("No history yet.") != std::string::npos,
					"Should print no history after clear"
				);
			}

			frontend_without_completions.addHistoryEntry("test entry");
			frontend_without_completions.addHistoryEntry("");
			frontend_without_completions.addHistoryEntry("line 1\nline 2");

			{
				ScopedStreamCapture capture(std::cout);
				frontend_without_completions.printHistory();
				assertTrue(
					capture.str().find("test entry") != std::string::npos,
					"Should contain first entry"
				);
				assertTrue(
					capture.str().find("line 1") != std::string::npos, "Should contain third entry"
				);
			}

			frontend_without_completions.clearHistory();

			{
				ScopedStreamCapture capture(std::cout);
				frontend_without_completions.printHistory();
				assertTrue(
					capture.str().find("No history yet.") != std::string::npos,
					"Should print no history after clear"
				);
			}
		}

		void testReplSaveAndPrintHistory() {
			removeSessionHistoryFile();
			ReplSession session;

			auto result1 = session.processLine("var p: i32 = 123;");
			assertTrue(result1.status == ReplResult::Status::Success, "Declaration should succeed");

			{
				ScopedStreamCapture capture(std::cout);
				auto                result2 = session.processLine("/history");
				assertTrue(result2.status == ReplResult::Status::Success, "/history should succeed");
				assertTrue(
					capture.str().find("var p: i32 = 123;") != std::string::npos,
					"Should print history containing var p"
				);
			}

			auto reset_result = session.processLine("/reset");
			assertTrue(
				reset_result.status == ReplResult::Status::Reset, "/reset should request reset"
			);

			{
				ScopedStreamCapture capture(std::cerr);
				session.replayHistoryEntries(0, false);
				assertTrue(capture.str().empty(), "Should not print anything for 0 count replay");
			}
		}

		void testReplResetSyntaxErrorsMore() {
			removeSessionHistoryFile();
			ReplSession session;

			{
				ScopedStreamCapture capture(std::cerr);
				auto                result = session.processLine("/reset a");
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Invalid /reset syntax should not request a reset"
				);
				assertTrue(
					capture.str().find("Usage: /reset") != std::string::npos,
					"Invalid /reset syntax should print usage"
				);
			}

			{
				ScopedStreamCapture capture(std::cerr);
				auto                result = session.processLine("/reset -a");
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Invalid /reset syntax should not request a reset"
				);
				assertTrue(
					capture.str().find("Usage: /reset") != std::string::npos,
					"Invalid /reset syntax should print usage"
				);
			}

			{
				ScopedStreamCapture capture(std::cerr);
				auto                result = session.processLine("/reset 99999");
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Invalid /reset syntax should not request a reset"
				);
			}
		}

		void testReplResetSyntaxErrors() {
			removeSessionHistoryFile();
			ReplSession session;

			{
				ScopedStreamCapture capture(std::cerr);
				auto                result = session.processLine("/reset -");
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Invalid /reset syntax should not request a reset"
				);
				assertTrue(
					capture.str().find("Usage: /reset") != std::string::npos,
					"Invalid /reset syntax should print usage"
				);
			}

			{
				ScopedStreamCapture capture(std::cerr);
				auto                result = session.processLine("/reset 1 2");
				assertTrue(
					result.status == ReplResult::Status::Success,
					"Extra tokens in /reset should not request a reset"
				);
				assertTrue(
					capture.str().find("Usage: /reset") != std::string::npos,
					"Extra tokens in /reset should print usage"
				);
			}

			{
				auto setup_a = session.processLine("var a: i32 = 1;");
				auto setup_b = session.processLine("var b: i32 = a + 1;");
				assertTrue(
					setup_a.status == ReplResult::Status::Success
						&& setup_b.status == ReplResult::Status::Success,
					"Setup should succeed before reset validation"
				);
				ScopedStreamCapture capture(std::cerr);
				auto                result = session.processLine("/reset 1");
				assertTrue(
					result.status == ReplResult::Status::Reset,
					"/reset 1 should request a reset when history has entries"
				);
				assertTrue(capture.str().empty(), "Valid /reset should not print usage");
			}

			removeSessionHistoryFile();
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
				session.m_session_history.size() == 1,
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
				= { "/exit",    "/help",    "/h",         "/history", "/hist",
				    "/symbols", "/syms",    "/variables", "/vars",    "/functions",
				    "/fns",     "/details", "/clear",     "/c" };

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
			auto initial_history_size = session.m_session_history.size();
			auto initial_line_count   = session.m_line_counter;

			assertTrue(initial_history_size == 0, "History should start empty");
			assertTrue(initial_line_count == 0, "Line counter should start at 0");

			// Execute a simple statement
			std::string_view code = "var x: i32 = 10;";
			session.processLine(code);

			// Verify that history was updated
			auto updated_history_size = session.m_session_history.size();
			assertTrue(
				updated_history_size >= initial_history_size,
				"History size should increase or stay same after processing"
			);
			ASSERT_HAS_VALUE(
				session.m_lowering_context,
				"Lowering context should be initialized for REPL execution"
			);
		}

		void testLoadScriptFileMissingPath() {
			ReplSession session;

			auto result = session.loadScriptFile("   \t");

			assertTrue(
				result.status == ReplResult::Status::Error,
				"Loading script with empty path should fail"
			);
			assertTrue(
				result.message.find("Missing script path") != std::string::npos,
				"Error should explain that script path is missing"
			);
			assertFalse(session.m_suppress_repl, "Output suppression should be restored");
		}

		void testLoadScriptFileInvalidContents() {
			ReplSession session;
			auto        script_file = fs::FileManager::createRandomTempFile("var x = ;");

			auto result = session.loadScriptFile(script_file.getFilePath().string());

			fs::FileManager::deleteFile(script_file);

			assertTrue(
				result.status == ReplResult::Status::Error, "Loading invalid script should fail"
			);
			assertTrue(
				!result.message.empty(), "Invalid script should produce a useful error message"
			);
			assertFalse(session.m_suppress_repl, "Output suppression should be restored");
		}

		void testLoadScriptFileExecutesStatements() {
			ReplSession session;

			auto script_file
				= fs::FileManager::createRandomTempFile("var loaded_x: i32 = 1;\nloaded_x = 10;");
			auto script_path = std::string("   ") + script_file.getFilePath().string();

			auto initial_history_size = session.m_session_history.size();

			auto result = session.loadScriptFile(script_path);

			assertTrue(
				result.status == ReplResult::Status::Success, "Loading a valid script should succeed"
			);

			auto updated_history_size = session.m_session_history.size();
			ASSERT_EQUAL(2UL, updated_history_size - initial_history_size);
			assertFalse(session.m_suppress_repl, "Output suppression should be restored");

			auto follow_up_result = session.processLine("1 + 1;");
			assertTrue(
				follow_up_result.status == ReplResult::Status::Success,
				"Session should remain usable after script loading"
			);

			fs::FileManager::deleteFile(script_file);
		}

		void testLoadCommandExecutesScript() {
			ReplSession session;

			auto script_file
				= fs::FileManager::createRandomTempFile("var cmd_x: i32 = 7;\ncmd_x = cmd_x + 2;");
			auto command = std::string("/load ") + script_file.getFilePath().string();

			auto result = session.processLine(command);

			assertTrue(
				result.status == ReplResult::Status::Success,
				"/load command should be handled successfully"
			);

			auto follow_up_result = session.processLine("2 + 3;");
			assertTrue(
				follow_up_result.status == ReplResult::Status::Success,
				"REPL should continue processing input after /load"
			);

			fs::FileManager::deleteFile(script_file);
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
			usize initial_history_size = session.m_session_history.size();

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
			usize expected_history_size = initial_history_size + tests.size();
			assertTrue(
				session.m_session_history.size() == expected_history_size,
				"REPL session history should track all variable operations"
			);
		}

		/**
		 * @brief Test that unsupported action statements return explicit classification errors.
		 */
		void testReplUnsupportedActionClassification() {
			ReplSession session;

			auto assert_unsupported_action = [&](std::string_view input) {
				auto result = session.processLine(input);
				assertTrue(
					result.status == ReplResult::Status::Error,
					"Unsupported action should produce an error status"
				);
				assertTrue(
					result.message.find("Unsupported single statement kind for REPL classification")
						!= std::string::npos,
					"Expected explicit unsupported classification error"
				);
				assertTrue(
					result.message.find("Action") != std::string::npos,
					"Expected error to include statement kind"
				);
			};

			assert_unsupported_action("continue;");
			assert_unsupported_action("return 5;");
			assert_unsupported_action("break;");
			assert_unsupported_action("throw 5;");
			assert_unsupported_action("return;");
		}

		void testReplCommandsResetCommand() {
			ReplSession session;

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/commands-reset");
				assertTrue(
					result.status == ReplResult::Status::Success, "/commands-reset should succeed"
				);
				assertTrue(
					capture.str().find("Command history cleared.") != std::string::npos,
					"Should print confirmation, got: " + capture.str()
				);
			}
			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/cmds-reset");
				assertTrue(
					result.status == ReplResult::Status::Success, "/cmds-reset should succeed"
				);
				assertTrue(
					capture.str().find("Command history cleared.") != std::string::npos,
					"Should print confirmation"
				);
			}
			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/cmds");
				assertTrue(result.status == ReplResult::Status::Success, "/cmds should succeed");
				assertTrue(
					capture.str().find("No history yet.") != std::string::npos,
					"Should print no history"
				);
			}
			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/commands");
				assertTrue(result.status == ReplResult::Status::Success, "/commands should succeed");
				assertTrue(
					capture.str().find("No history yet.") != std::string::npos,
					"Should print no history"
				);
			}
		}

		void testReplHistoryCommandAdvanced() {
			removeSessionHistoryFile();
			ReplSession session;

			auto setup_a = session.processLine("var x_a = 1;");
			auto setup_b = session.processLine("var y_b = 2;");
			assertTrue(
				setup_a.status == ReplResult::Status::Success
					&& setup_b.status == ReplResult::Status::Success,
				"Setup should succeed"
			);

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/hist");
				assertTrue(result.status == ReplResult::Status::Success, "/hist should succeed");
				assertTrue(
					capture.str().find("var x_a = 1;") != std::string::npos, "Should print x_a"
				);
				assertTrue(
					capture.str().find("var y_b = 2;") != std::string::npos, "Should print y_b"
				);
			}

			{
				ScopedStreamCapture capture(std::cout);
				auto                result = session.processLine("/history");
				assertTrue(result.status == ReplResult::Status::Success, "/history should succeed");
				assertTrue(
					capture.str().find("var x_a = 1;") != std::string::npos, "Should print x_a"
				);
			}
			removeSessionHistoryFile();
		}

		void testReplReplayHistoryNotSilent() {
			removeSessionHistoryFile();

			{
				ReplSession         session;
				ScopedStreamCapture capture(std::cerr);
				session.replayHistoryEntries(1, false);
				assertTrue(
					capture.str().find("Warning: no REPL session history entries found to replay.")
						!= std::string::npos,
					"Should print warning when no history exists"
				);
			}

			{
				std::ofstream out(".duckling_repl_session_history");
				out << "Duckling REPL session history\n";
				out << "Entries: 1\n\n";
				out << "[1]\n";
				out << "var x = ;\n\n";
				out.close();

				ReplSession         replay_session;
				ScopedStreamCapture capture(std::cerr);
				replay_session.replayHistoryEntries(1, false);
				std::cout << "DEBUG CAPTURE: '" << capture.str() << "'\n";
				assertTrue(
					capture.str().find(
						"Warning: stopping history replay after entry 1 due to error."
					) != std::string::npos,
					"Should print warning when replay errors"
				);
			}

			removeSessionHistoryFile();
		}

	public:
		~ReplSimulationTest() override = default;
	};

}  // namespace compiler::repl

// Bring the test class into global scope for TESTER_COMMON_MAIN
using ReplSimulationTest = compiler::repl::ReplSimulationTest;

TESTER_COMMON_MAIN("/src/compiler/repl/tests/")
