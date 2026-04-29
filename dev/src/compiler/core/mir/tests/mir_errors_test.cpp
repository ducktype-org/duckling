#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/query_result.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class HeliosErrorsTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosErrorsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testErrorLogging); }

private:
	/**
	 * @brief Helper function that check for MIR compilation
	 * errors in a module with given content. Requires that the HELIOS step passes.
	 *
	 * It creates a virtual file from the `module_content` argument
	 * and creates a module tree from it every function call.

	 * @param module_content The content of the module main source file.
	 * @param present_phrases List of phrases that should be present in the logged errors.
	 * @param logged_msg_count Expected number of logged error messages.
	 */
	void checkForErrorOnCompileModule(
		std::string_view                     module_content,
		const std::vector<std::string_view>& present_phrases,
		u64                                  logged_msg_count
	) {
		frontend::ModuleID module_id
			= frontend::createModuleTreeFromContents(module_content, "test_package");
		query::utils::withContextDo([&](query::Context& ctx) {
			auto hout_result = ctx.query<helios::QueryModuleHOUT>(module_id);
			assertTrue(
				hout_result->hasValue(), "Expected HOUT query to succeed for module content."
			);
			auto logger = query::Context::dumpToOneLoggerAndClear();
			assertTrue(!logger->hasErrors(), "Expected no errors to be logged by HELIOS.");

			bool some_fun_lowering_failed = false;
			for (auto fun: hout_result->valueOrPanic().functions) {
				auto result = ctx.query<mir::LowerToMIRFunction>({ fun });
				if (result->hasFailed()) some_fun_lowering_failed = true;
			}
			assertTrue(
				some_fun_lowering_failed, "Expected some MIR query to fail for module functions."
			);
			logger = query::Context::dumpToOneLoggerAndClear();
			assertTrue(logger->hasErrors(), "Expected errors to be logged by MIR.");

			std::stringstream logged_messages;
			logger->terminalPrint(logged_messages);
			std::cerr << "Logged messages:\n" << logged_messages.str() << "\n";
			auto msg_count = logger->messageCount();
			assertEqual(
				msg_count,
				logged_msg_count,
				"Expected logged message count to be " + std::to_string(logged_msg_count)
					+ ", but got " + std::to_string(msg_count)
			);
			for (const auto& phrase: present_phrases) {
				std::string logged_str = logged_messages.str();
				assertTrue(
					logged_str.find(phrase.data()) != std::string::npos,
					"Expected logged messages to contain phrase: " + std::string(phrase)
				);
			}
		});
	}

	void testErrorLogging() {
		// ============================ Unused shadowed variable ============================
		checkForErrorOnCompileModule(
			R"(fun shadowedLocal() = {
                var n = 42;
                if (true) {
                    var n = 24;
                }
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
		checkForErrorOnCompileModule(
			R"(fun shadowedArg(n: i64) = {
                var n = 42;
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
