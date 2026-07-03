/**
 * @file mir_lifetime_test.cpp
 * @brief Tests for MIR lifetime analysis and validation
 */

#include "utils/lifetime_checker.hpp"
#include "utils/test_utils.hpp"

#include <ctv/ctv.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_validation.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using namespace compiler::mir::test_utils;
using compiler::mir::BlockID;
using query::utils::withContextDo;

class MIRLifetimeTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRLifetimeTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(lifetimeAnalysisTest);
		TESTER_ADD_TEST(moveValidationTest);
		TESTER_ADD_TEST(reinitAfterMoveTest);
		TESTER_ADD_TEST(moveDestructorTest);
		TESTER_ADD_TEST(simpleLifetimeSequenceTest);
		TESTER_ADD_TEST(lifetimeFlagsRepeatedBlocks);
		TESTER_ADD_TEST(lifetimeFlagsSingleBlock);
		TESTER_ADD_TEST(lifetimeFlagsNestedBlocks);
		TESTER_ADD_TEST(newIntermediateBlockForDifferentEndingScopesTest);
		TESTER_ADD_TEST(multipleIntermediateBlocksTest);
		TESTER_ADD_TEST(optimizationForSameEndingScopesTest);
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	void lifetimeAnalysisTest() {
		// Basic test that lifetime analysis runs without throwing exceptions
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		auto foo_mir = getMIRFunctionByName(module, "foo");

		ASSERT_EQUAL(foo_mir->local_list.size(), 5);

		// Verify lifetime scopes are assigned
		for (const auto& local: foo_mir->local_list) ASSERT_HAS_VALUE(local.scope);
	}

	void simpleLifetimeSequenceTest() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		auto goo_mir = getMIRFunctionByName(module, "goo");

		LifetimeChecker checker;
		checker.expectConstruct("a")
			.expectInstruction(compiler::mir::Operation::Call)
			.expectDestruct("a")
			.validate(goo_mir);
	}

	void lifetimeFlagsRepeatedBlocks() {
		auto [module, scope]
			= getModule(fs::File(path("modules/lifetime_flags/repeated_blocks.dmf")));
		auto            foo_mir = getMIRFunctionByName(module, "main");
		LifetimeChecker checker;
		checker.expectConstruct("a")
			.expectScopeStart("a")

			.expectConstruct("b")
			.expectScopeStart("b")
			.expectDestruct("b")
			.expectScopeEnd("b")

			.expectConstruct("c")
			.expectScopeStart("c")
			.expectDestruct("c")
			.expectScopeEnd("c")

			.expectScopeStart("4.tmp")  // The if condition temporary
			.expectInstruction(compiler::mir::Operation::Branch)
			.expectScopeEnd("4.tmp")
			.expectDestruct("a")
			.expectInstruction(compiler::mir::Operation::ReturnValue)
			.expectScopeEnd("a")
			// Second return: `return a` moves `a` out, so it is moved (not destructed) here.
			.expectMove("a")
			.expectInstruction(compiler::mir::Operation::ReturnValue)
			.expectScopeEnd("a")
			.validate(foo_mir);

		auto return_value = foo_mir->local_list[3];
		assertTrue(
			return_value->lifetime_flags.contains(compiler::mir::LifetimeFlag::ReturnTmpValue),
			"Return value should have ReturnTmpValue flag"
		);
		auto condition_tmp = foo_mir->local_list[4];
		assertTrue(
			condition_tmp->lifetime_flags.contains(compiler::mir::LifetimeFlag::ConditionTmpValue),
			"Condition temporary should have ConditionTmpValue flag"
		);
	}

	void lifetimeFlagsSingleBlock() {
		auto [module, scope] = getModule(fs::File(path("modules/lifetime_flags/single_block.dmf")));
		auto            main = getMIRFunctionByName(module, "main");
		LifetimeChecker checker;
		checker.expectConstruct("x")
			.expectScopeStart("x")
			.expectScopeStart("y")
			.expectScopeStart("z")

			.expectConstruct("y")
			.expectConstruct("z")

			.expectDestruct("z")
			.expectDestruct("y")
			.expectDestruct("x")

			.expectScopeEnd("z")
			.expectScopeEnd("y")
			.expectScopeEnd("x")
			.validate(main);
	}

	void lifetimeFlagsNestedBlocks() {
		auto [module, scope]
			= getModule(fs::File(path("modules/lifetime_flags/nested_blocks.dmf")));
		auto            main = getMIRFunctionByName(module, "main");
		LifetimeChecker checker;
		checker.expectConstruct("x")
			.expectScopeStart("x")

			.expectConstruct("y")
			.expectScopeStart("y")

			.expectConstruct("z")
			.expectScopeStart("z")

			.expectDestruct("z")
			.expectScopeEnd("z")

			.expectDestruct("y")
			.expectScopeEnd("y")

			.expectDestruct("x")
			.expectScopeEnd("x")
			.validate(main);
	}

	void moveValidationTest() {
		// Use-after-move validation, driven by real `move` expressions. `good*` functions lower
		// successfully (and we check the move/destructor shape they produce), while `bad*`
		// functions fail to lower because a moved (or possibly-moved) value is read.
		using compiler::mir::Operation;
		auto [module, scope] = getModule(fs::File(path("modules/move_validation")));

		// In all `good*` functions: Local(0) = param `c`, Local(1) = `a`, Local(2) = `b`.

		// good1: straight-line move. `b` is moved (so it has no destructor), while `a` stays alive
		// to its scope end and gets an unconditional `Destruct`.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good1"));

		// good2: `b` is moved only on the `if` branch, so at the merge it is `MaybeMoved` and gets
		// a conditional `DestructIf`; `a` still gets an unconditional `Destruct`.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::DestructIf)
			.expectDestruct("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good2"));

		// good3: `b` is loop-local and moved on a conditional path inside the loop body, so it is
		// `MaybeMoved` at the end of the body -> `DestructIf`.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::DestructIf)
			.expectDestruct("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good3"));

		// good4: `b` is loop-local and moved unconditionally in the body, so it is `Moved` at the
		// end of the body and gets no destructor at all; only `a` is destructed.
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")
			.expectInstruction(Operation::Destruct)
			.expectDestruct("a")
			.validate(getMIRFunctionByName(module, "good4"));

		// `bad*` functions read a moved / possibly-moved value, so lowering must fail.
		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			auto check_fails = [&](std::string_view name) {
				for (CRef<compiler::helios::HOUTFunction> fun: unit.functions) {
					if (fun->declaration->original_name.strView() != name) continue;
					assertTrue(
						ctx.query<compiler::mir::LowerToMIRFunction>({ fun })->hasFailed(),
						base::strConcat(name, " should fail to lower")
					);
					return;
				}
				CORE_PANIC(base::strConcat("Function '", name, "' not found in module"));
			};

			check_fails("bad1");
			check_fails("bad2");
			check_fails("bad3");
		});
	}

	void reinitAfterMoveTest() {
		// Reinitialization by assignment. A bare-local store (e.g. `b = 99`) carries a `Reinit`
		// flag that revives the local for liveness, so reading it after a prior move-out is valid.
		// Each `getMIRFunctionByName` below panics if lowering fails, so the mere fact these
		// `reinit*` functions lower is the regression guard against the false use-after-move.
		using compiler::mir::Operation;
		auto [module, scope] = getModule(fs::File(path("modules/move_validation")));

		// reinit1: `b` moved out (`a = move b`), reinitialized (`b = 99`), then read (`eat(b)`).
		// The `a = move b` store also reinitializes `a`.
		auto reinit1 = getMIRFunctionByName(module, "reinit1");
		LifetimeChecker{}
			.expectConstruct("a")
			.expectConstruct("b")
			.expectMove("b")                                    // a = move b
			.expectReinit("b")                                  // b = 99
			.validate(reinit1);
		LifetimeChecker{}.expectReinit("a").validate(reinit1);  // a = move b

		// reinit2: `a` is moved and reinitialized on both branches, alive at the merge read.
		auto reinit2 = getMIRFunctionByName(module, "reinit2");
		LifetimeChecker{}.expectMove("a").expectReinit("a").validate(reinit2);

		// reinit3: `a` is moved then reinitialized inside the loop body.
		auto reinit3 = getMIRFunctionByName(module, "reinit3");
		LifetimeChecker{}.expectMove("a").expectReinit("a").validate(reinit3);
	}

	void moveDestructorTest() {
		// Move-aware destructor insertion. With the liveness analysis in place, a local that is
		// definitely alive at its scope end gets an unconditional `Destruct`, while a local that
		// is moved on only some control-flow paths is `MaybeMoved` and gets a conditional
		// `DestructIf`. A definitely-moved local gets no destructor at all.
		auto [module, scope] = getModule(fs::File(path("modules/move_lifetime")));

		// `a` is never moved -> alive at scope end -> unconditional `Destruct`.
		auto no_move = getMIRFunctionByName(module, "noMove");
		LifetimeChecker{}
			.expectConstruct("a")
			.expectInstruction(compiler::mir::Operation::Destruct)
			.expectDestruct("a")
			.validate(no_move);

		// `a` is moved on the `if` branch only -> `MaybeMoved` at the merge -> `DestructIf`.
		auto maybe_move = getMIRFunctionByName(module, "maybeMove");
		LifetimeChecker{}
			.expectConstruct("a")
			.expectMove("a")
			.expectInstruction(compiler::mir::Operation::DestructIf)
			.expectDestruct("a")
			.validate(maybe_move);
	}

	i32 countIntermediateDestructorBlocks(CRef<compiler::mir::Function> func) {
		i32 intermediate_blocks_count = 0;
		for (const auto& block_id: func->block_order) {
			const auto& block = func->blocks.at(block_id);
			if (!block->instructions.empty()
			    && block->instructions[0].operation == compiler::mir::Operation::Nop) {
				if (block->terminator.operation == compiler::mir::Operation::Jump)
					intermediate_blocks_count++;
			}
		}
		return intermediate_blocks_count;
	}

	void newIntermediateBlockForDifferentEndingScopesTest() {
		auto [module, scope] = getModule(fs::File(path("modules/destructor_insertion")));
		auto a_mir           = getMIRFunctionByName(module, "A");
		ASSERT_EQUAL_PRINT(2, countIntermediateDestructorBlocks(a_mir));
	}

	void multipleIntermediateBlocksTest() {
		auto [module, scope] = getModule(fs::File(path("modules/destructor_insertion")));
		auto b_mir           = getMIRFunctionByName(module, "B");
		ASSERT_EQUAL_PRINT(4, countIntermediateDestructorBlocks(b_mir));
	}

	void optimizationForSameEndingScopesTest() {
		auto [module, scope] = getModule(fs::File(path("modules/destructor_insertion")));
		auto c_mir           = getMIRFunctionByName(module, "C");
		ASSERT_EQUAL_PRINT(0, countIntermediateDestructorBlocks(c_mir));
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
