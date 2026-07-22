#include "utils/test_utils.hpp"

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
		TESTER_ADD_TEST(testGeneratedSymbolShadowingDoesNotError);
	}

private:
	void testErrorLogging() {
		// ============================ Unused shadowed variable ============================
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun shadowedLocal() = {
                var n = 42;
                if (true) {
                    var n = 24;
                }
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun shadowedArg(n: i64) = {
                var n = 42;
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
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun eat(x: i64) = x;
               fun twoPlaces(a: i64) = {
                   if (a == 0) { eat(move a); }
                   else { eat(move a); }
                   eat(a);
               })",
			{ "is used after it has been moved out of.", "Value moved here." },
			1
		);

		// ===================== Move from a non-local place (NYI) =====================
		compiler::mir::test_utils::checkForErrorOnCompileModule(
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
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun useBeforeInitLoop() = {
                   let x = 0;
                   while (x > 0) {
                       a + 1;
                       let a = 0;
                   }
                   return;
               })",
			{ "is used before it is initialized" },
			1
		);

		// `c` is read before its declaration further down the function, and is also assigned
		// (`c = 1`) before that declaration — the read is reported as a use-before-init and the
		// assignment as a write-before-init, so two diagnostics are emitted.
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun useBeforeInitBranch(x: i64) = {
                   c + 1;
                   if (x > 0) {
                       c = 1;
                   }
                   var c = 20;
                   return;
               })",
			{ "is used before it is initialized", "is used before it is initialized" },
			2
		);

		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun writeBeforeInit() = {
                   n = 5;
                   var n: i64 = 0;
                   return;
               })",
			{ "is used before it is initialized" },
			1
		);

		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(class Cls { x: i64; }
               fun writeBeforeInitField() = {
                   a.x = 20;
                   var a: Cls = Cls(0);
                   return;
               })",
			{ "is used before it is initialized" },
			1
		);

		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(class Cls { x: i64; }
               fun writeBeforeInitField() = {
					var a: Cls = Cls(0);
					let b = move a;
					a.x = 20;
					return;
               })",
			{ "is used after it has been moved out of" },
			1
		);
	}

	/**
	 * @brief Regression test for #2307: classes with fields named "__result" must compile.
	 *
	 * The compiler generates a `__result` local variable inside implicit constructors.
	 * This must not collide with a user-defined field of the same name.
	 * Before the fix, validateShadowing() flagged this as an illegal shadow and
	 * compilation failed.
	 */
	void testGeneratedSymbolShadowingDoesNotError() {
		// #2307: Classes with fields resembling generated symbol names must compile.
		compiler::mir::test_utils::checkForNoErrorOnCompileModule(
			R"(class T { __result: i64; other: i64; }
               var g: T = T(1, 2);
               fun main() -> i64 = { return g.__result; })"
		);
		compiler::mir::test_utils::checkForNoErrorOnCompileModule(
			R"(class T { _result: i64; other: i64; }
               var g: T = T(1, 2);
               fun main() -> i64 = { return g._result; })"
		);
		compiler::mir::test_utils::checkForNoErrorOnCompileModule(
			R"(class T { ___result: i64; other: i64; }
               var g: T = T(1, 2);
               fun main() -> i64 = { return g.___result; })"
		);
		compiler::mir::test_utils::checkForNoErrorOnCompileModule(
			R"(class T { result: i64; other: i64; }
               var g: T = T(1, 2);
               fun main() -> i64 = { return g.result; })"
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
