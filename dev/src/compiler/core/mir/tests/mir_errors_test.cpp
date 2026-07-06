#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

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
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testErrorLogging);
		TESTER_ADD_TEST(testMoveErrors);
		TESTER_ADD_TEST(testUseBeforeInit);
	}

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

			auto mir_result = mir::lowerToMIRUnit(ctx, &hout_result->valueOrPanic());
			assertTrue(
				mir_result.hasFailed(), "Expected some MIR query to fail for module lowering."
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
		checkForErrorOnCompileModule(
			R"(fun shadowedVar() = {
				var arr: i32[2];
                var x = 42;
				for (x in arr) {}
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
		checkForErrorOnCompileModule(
			R"(fun shadowedIter() = {
				var arr: i32[2];
				for (x in arr) {
					for (x in arr) {}
				}
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
	}

	void testMoveErrors() {
		// Note: the basic single-path use-after-move case is covered by mir_lifetime_test's
		// moveValidationTest (bad1). Here we focus on the rendered diagnostics that are specific
		// to certain shapes.

		// ====================== Moved on two control-flow paths ======================
		// Both branches move `a`, so the use at the merge is reported once but points at both
		// move sites via two `value moved here` notes.
		checkForErrorOnCompileModule(
			R"(fun eat(x: i64) = x;
               fun twoPlaces(a: i64) = {
                   if (a == 0) { eat(move a); }
                   else { eat(move a); }
                   eat(a);
               })",
			{ "Use of a moved value.", "value moved here" },
			1
		);

		// ===================== Move from a non-local place (NYI) =====================
		checkForErrorOnCompileModule(
			R"(class Cls { x: i64; }
               fun moveField() = {
                   let a = Cls(10);
                   move a.x;
               })",
			{ "Moving from a non-local place is not supported yet." },
			1
		);
	}

	void testUseBeforeInit() {
		// `a` is read at the top of the loop body before its declaration; on every iteration it
		// is uninitialized at that point.
		checkForErrorOnCompileModule(
			R"(fun useBeforeInitLoop() = {
                   let x = 0;
                   while (x > 0) {
                       a + 1;
                       let a = 0;
                   }
                   return;
               })",
			{ "Use of an uninitialized value.", "is used before it is initialized" },
			1
		);

		// `c` is read before its declaration further down the function.
		checkForErrorOnCompileModule(
			R"(fun useBeforeInitBranch(x: i64) = {
                   c + 1;
                   if (x > 0) {
                       c = 1;
                   }
                   var c = 20;
                   return;
               })",
			{ "Use of an uninitialized value.", "is used before it is initialized" },
			1
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
