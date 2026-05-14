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
		TESTER_ADD_TEST(simpleLifetimeSequenceTest);
		TESTER_ADD_TEST(lifetimeFlagsRepeatedBlocks);
		TESTER_ADD_TEST(lifetimeFlagsSingleBlock);
		TESTER_ADD_TEST(lifetimeFlagsNestedBlocks);
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	void lifetimeAnalysisTest() {
		// Basic test that lifetime analysis runs without throwing exceptions
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		auto foo_mir = getMIRFunctionByName(module, "foo");

		ASSERT_EQUAL(foo_mir->local_list.size(), 5);

		// Verify lifetime scopes are assigned
		for (const auto& local: foo_mir->local_list) ASSERT_TRUE(local.scope.has_value());
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
			.expectInstruction(compiler::mir::Operation::ReturnValue)
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
		// @note This test is very fragile and may require hotfixes even after unrelated changes.
		// Proper tests can be written once 'move' is implemented. It should contain usage of 'if',
		// 'else', 'break', 'continue', 'switch' etc..
		auto [module, scope] = getModule(fs::File(path("modules/move_validation")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;

			for (CRef<compiler::helios::HOUTFunction> fun: functions) {
				if (fun->declaration->original_name.str() == "good1") {
					auto& mir_rep_good1 = (compiler::mir::Function&) ctx
					                          .query<compiler::mir::LowerToMIRFunction>({ fun })
					                          ->valueOrThrow();


					CRef<compiler::mir::MIRLocal> tmp(mir_rep_good1.local_list[2]);

					mir_rep_good1.blocks[mir_rep_good1.block_order[0]]
						.instructions[4]
						.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_good1).isOk());
				}

				if (fun->declaration->original_name.str() == "good2") {
					auto& mir_rep_good2 = (compiler::mir::Function&) ctx
					                          .query<compiler::mir::LowerToMIRFunction>({ fun })
					                          ->valueOrThrow();

					CRef<compiler::mir::MIRLocal> var_b(mir_rep_good2.local_list[2]);
					compiler::mir::Instruction&   assignment
						= mir_rep_good2.blocks[mir_rep_good2.block_order[1]].instructions[0];

					// If this test fails use the following to find the correct Instruction.
					// mir_rep_good2.debugPrint(std::cerr);
					// assignment.debugPrint(std::cerr);
					assertEqual(
						compiler::mir::Operation::Assign,
						assignment.operation,
						"Fragile test, please fix (good2, assignment)"
					);
					assertTrue(
						assignment.arguments[0].isLocal(), "Fragile test, please fix (good2, local)"
					);
					assignment.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, var_b);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_good2).isOk());
				}

				if (fun->declaration->original_name.str() == "good3") {
					auto& mir_rep_good3 = (compiler::mir::Function&) ctx
					                          .query<compiler::mir::LowerToMIRFunction>({ fun })
					                          ->valueOrThrow();

					CRef<compiler::mir::MIRLocal> tmp(mir_rep_good3.local_list[2]);

					mir_rep_good3.blocks[mir_rep_good3.block_order[2]]
						.instructions[2]
						.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_good3).isOk());
				}

				if (fun->declaration->original_name.str() == "good4") {
					auto& mir_rep_good4 = (compiler::mir::Function&) ctx
					                          .query<compiler::mir::LowerToMIRFunction>({ fun })
					                          ->valueOrThrow();

					CRef<compiler::mir::MIRLocal> tmp(mir_rep_good4.local_list[2]);

					mir_rep_good4.blocks[mir_rep_good4.block_order[1]]
						.instructions[2]
						.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_good4).isOk());
				}

				if (fun->declaration->original_name.str() == "bad1") {
					auto& mir_rep_bad1 = (compiler::mir::Function&) ctx
					                         .query<compiler::mir::LowerToMIRFunction>({ fun })
					                         ->valueOrThrow();

					CRef<compiler::mir::MIRLocal> tmp(mir_rep_bad1.local_list[2]);

					mir_rep_bad1.blocks[mir_rep_bad1.block_order[0]]
						.instructions[2]
						.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_bad1).isBad());
				}

				if (fun->declaration->original_name.str() == "bad2") {
					auto& mir_rep_bad2 = (compiler::mir::Function&) ctx
					                         .query<compiler::mir::LowerToMIRFunction>({ fun })
					                         ->valueOrThrow();

					CRef<compiler::mir::MIRLocal> tmp(mir_rep_bad2.local_list[2]);
					mir_rep_bad2.blocks[mir_rep_bad2.block_order[1]]
						.instructions[0]
						.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_bad2).isBad());
				}

				if (fun->declaration->original_name.str() == "bad3") {
					auto& mir_rep_bad3 = (compiler::mir::Function&) ctx
					                         .query<compiler::mir::LowerToMIRFunction>({ fun })
					                         ->valueOrThrow();

					CRef<compiler::mir::MIRLocal> tmp(mir_rep_bad3.local_list[2]);
					mir_rep_bad3.blocks[mir_rep_bad3.block_order[1]]
						.instructions[0]
						.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

					ASSERT_TRUE(validateFunction(ctx, mir_rep_bad3).isBad());
				}
			}
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
