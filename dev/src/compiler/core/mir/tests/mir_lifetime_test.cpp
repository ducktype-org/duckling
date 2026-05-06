/**
 * @file mir_lifetime_test.cpp
 * @brief Tests for MIR lifetime analysis and validation
 */

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

#include "lifetime_validator.hpp"

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
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	void lifetimeAnalysisTest() {
		// Basic test that lifetime analysis runs without throwing exceptions
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;
			ASSERT_EQUAL(3, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0)->declaration->original_name);

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(0) })->valueOrThrow();

			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));
			ASSERT_EQUAL(foo_mir.local_list.size(), 5);

			// Verify lifetime scopes are assigned
			for (const auto& local: foo_mir.local_list) {
				ASSERT_TRUE(local.scope.has_value());
			}
		});
	}

	void simpleLifetimeSequenceTest() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& functions = unit.functions;

			// Test simple variable "goo" function
			auto& goo_mir = ctx.query<compiler::mir::LowerToMIRFunction>({ functions.at(1) })
			                    ->valueOrThrow();

			ASSERT_EQUAL(goo_mir.name, base::StrID("goo"));

			LifetimeValidator validator;
			validator.expectConstruct("a")
				.expectConstruct("b")
				.expectInstruction(compiler::mir::Operation::Call)
				.validate(goo_mir);
		});
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

					CRef<compiler::mir::MIRLocal> tmp(mir_rep_good2.local_list[3]);
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
					assignment.flags.emplace_back(compiler::mir::OperationFlag::Flag::Move, tmp);

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
