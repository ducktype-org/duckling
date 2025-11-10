/**
 * @file mir_tests.cpp
 */

#include <helios/queries.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_validation.hpp>
#include <mir/mir_structure/mir_structure.hpp>
#include <typesystem/higher/queries.hpp>

#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace tsh;
using namespace compiler::helios::test_utils;
using compiler::mir::BlockID;
using query::utils::withContextDo;

class MIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(simpleVarTest);
		TESTER_ADD_TEST(testTerminatorSuccessors);
		TESTER_ADD_TEST(mockLifetimeAnalysisTest);
		TESTER_ADD_TEST(simpleBools);
		TESTER_ADD_TEST(simpleFunctionCalls);
		TESTER_ADD_TEST(functionParametersTest);
		TESTER_ADD_TEST(functionEndTest);
		TESTER_ADD_TEST(moveValidation);
	}

private:
	void simpleTest() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_simple_test")));

		withContextDo([&](query::Context& ctx) {
			auto unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module);

			auto& functions = unit->functions;
			ASSERT_EQUAL(4, functions.size());
			ASSERT_EQUAL(base::StrID("foo1"), functions.at(0).declaration->original_name);
			ASSERT_EQUAL(base::StrID("foo2"), functions.at(1).declaration->original_name);
			ASSERT_EQUAL(base::StrID("foo3"), functions.at(2).declaration->original_name);
			ASSERT_EQUAL(base::StrID("foo4"), functions.at(3).declaration->original_name);

			auto foo1_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(0));
			auto foo2_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(1));
			auto foo3_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(2));
			auto foo4_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(3));

			ASSERT_EQUAL(foo1_mir.name, base::StrID("foo1"));
			ASSERT_EQUAL(foo2_mir.name, base::StrID("foo2"));
			ASSERT_EQUAL(foo3_mir.name, base::StrID("foo3"));
			ASSERT_EQUAL(foo4_mir.name, base::StrID("foo4"));

			ASSERT_EQUAL(foo1_mir.block_order.size(), 1);
			ASSERT_EQUAL(foo2_mir.block_order.size(), 2);
			ASSERT_EQUAL(foo3_mir.block_order.size(), 6);
			ASSERT_EQUAL(foo4_mir.block_order.size(), 2);

			// This doesn't test much other then that the code doesn't crash/throw exceptions.
			// It also make debug_prints covered by tests.
			std::stringstream all_functions;
			foo1_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo1_mir.validateBlockIDs().isOk());
			foo2_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo2_mir.validateBlockIDs().isOk());
			foo3_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo3_mir.validateBlockIDs().isOk());
			foo4_mir.debugPrint(all_functions);
			ASSERT_TRUE(foo4_mir.validateBlockIDs().isOk());
		});
	}

	void simpleVarTest() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(3, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).declaration->original_name);

			auto globals = unit->glob_data;
			ASSERT_EQUAL(2, globals.size());
			ASSERT_EQUAL(base::StrID("c"), globals.at(0).original_name);

			auto& c_ctor
				= ctx.query<compiler::mir::LowerGlobalDataToMirCtor>({ globals.at(0) })->value();
			ASSERT_TRUE(c_ctor.name.strView() == "constructor_of_c");

			auto foo_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(0));
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));

			// Test locals:
			ASSERT_EQUAL(foo_mir.local_list.size(), 3);

			auto i32_type = ctx.query<QueryIntegralType>(64);

			{
				auto a = foo_mir.local_list[0];
				ASSERT_EQUAL(a->getName(), "a");
				ASSERT_EQUAL(a->type.getType(), i32_type);
			}
			{
				auto b = foo_mir.local_list[1];
				ASSERT_EQUAL(b->getName(), "b");
				ASSERT_EQUAL(b->type.getType(), i32_type);
			}
			{
				auto b = foo_mir.local_list[2];
				ASSERT_EQUAL(b->getName(), "b");
				ASSERT_EQUAL(b->type.getType(), i32_type);
			}

			// Test code generation:

			ASSERT_EQUAL(foo_mir.block_order.size(), 8);

			// @note: instruction count does not include terminator instruction:

			using enum compiler::mir::Operation;

			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].id, foo_mir.block_order[0]);

			// those assertions might change when we improve mir generaration:
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].instructions.size(), 2);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].instructions.at(0).operation, Assign);
			// Check that the first instruction assigns to a global
			{
				const auto& instr = foo_mir.blocks[BlockID(7)].instructions.at(0);
				ASSERT_TRUE(std::holds_alternative<compiler::mir::MirGlobal>(instr.output.value()));
			}
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].instructions.at(1).operation, Assign);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(7)].terminator.operation, Jump);

			ASSERT_EQUAL(foo_mir.blocks[BlockID(6)].instructions.size(), 1);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(6)].instructions.at(0).operation, Assign);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(6)].terminator.operation, Jump);

			ASSERT_EQUAL(foo_mir.blocks[BlockID(5)].terminator.operation, Branch);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(4)].terminator.operation, Branch);

			ASSERT_EQUAL(foo_mir.blocks[BlockID(3)].instructions.size(), 1);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(3)].instructions.at(0).operation, Assign);
			ASSERT_EQUAL(foo_mir.blocks[BlockID(3)].terminator.operation, Jump);

			// Test debug print:
			// Note that doesn't test much other then that the code doesn't crash/throw exceptions.
			std::stringstream foo_str;
			foo_mir.debugPrint(foo_str);
			ASSERT_TRUE(foo_mir.validateBlockIDs().isOk());

			// Simple assignment tests
			auto goo_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(1));
			ASSERT_EQUAL(goo_mir.name, base::StrID("goo"));

			ASSERT_EQUAL(goo_mir.local_list.size(), 2);

			// assert that in the first block we have an assignment
			ASSERT_EQUAL(goo_mir.block_order.size(), 1);

			auto first_block_id = goo_mir.block_order[0];
			/**
	Local(5) :=  Cast             23               Flags[], Params{from:const i64, to:i32}, scope:40
			     Nop                               Flags[], Params{}, scope:40
	Local(5) :=  Cast             24               Flags[], Params{from:const i64, to:i32}, scope:39
			     Nop                               Flags[], Params{}, scope:39
	Local(6) :=  Call             Function(hoo)    Flags[Construct Local(6)], Params{}, scope:38
	Local(6) :=  Assign           15               Flags[], Params{}, scope:38
			     FunctionEnd                       Flags[], Params{}, scope:36
			 */
			ASSERT_EQUAL(goo_mir.blocks[first_block_id].instructions.size(), 6);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(0).operation,
				compiler::mir::Operation::Cast
			);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(2).operation,
				compiler::mir::Operation::Cast
			);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(4).operation,
				compiler::mir::Operation::Call
			);
			ASSERT_EQUAL(
				goo_mir.blocks[first_block_id].instructions.at(5).operation,
				compiler::mir::Operation::Assign
			);
		});
	}

	void testTerminatorSuccessors() {
		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(3, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).declaration->original_name);

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) })->value();

			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));
			ASSERT_EQUAL(foo_mir.block_order.size(), 7);
			ASSERT_EQUAL(foo_mir.local_list.size(), 3);

			auto get_block_terminator
				= [&](u64 block_id) { return foo_mir.blocks[BlockID(block_id)].terminator; };
			auto get_block_successors = [&](u64 block_id) {
				return getTerminatorSuccessors(get_block_terminator(block_id));
			};

			using BlockList = std::vector<BlockID>;
			ASSERT_EQUAL(get_block_successors(1), BlockList{});
			ASSERT_EQUAL(get_block_successors(2), BlockList{ BlockID{ 1 } });


			ASSERT_EQUAL(get_block_successors(3), BlockList{ BlockID{ 1 } });

			// Here, the order does not matter.
			// If it breaks because the order changes,
			// the check has to be changed to an order-free assertion.
			ASSERT_EQUAL(get_block_successors(4), BlockList{ BlockID{ 3 } COMMA BlockID{ 2 } });
			ASSERT_EQUAL(get_block_successors(5), BlockList{ BlockID{ 6 } COMMA BlockID{ 4 } });

			ASSERT_EQUAL(get_block_successors(6), BlockList{ BlockID{ 5 } });
			ASSERT_EQUAL(get_block_successors(7), BlockList{ BlockID{ 5 } });
		});
	}

	void mockLifetimeAnalysisTest() {
		// since lifetime analysis is a mock implementation, we don't
		// yet test them with much effort.
		// @TODO: add better tests once proper lifetimes implementation is in place
		// But we do want to make sure, that it compiles and does not throw:

		auto [module, scope] = getModule(fs::File(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(3, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).declaration->original_name);

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) })->value();

			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));
			ASSERT_EQUAL(foo_mir.local_list.size(), 3);
		});
	}

	void simpleBools() {
		auto [module, scope] = getModule(fs::File(path("modules/booleans")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(2, functions.size());

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) })->value();
			ASSERT_TRUE(foo_mir.validateBlockIDs().isOk());

			// note: it might change where those branch operations are placed:
			// if this happens, just see mir-output of tested module for mir block numbers
			auto true_mir_value  = foo_mir.blocks[BlockID(6)].terminator.arguments.at(0);
			auto false_mir_value = foo_mir.blocks[BlockID(3)].terminator.arguments.at(0);

			ASSERT_EQUAL(true_mir_value.get<compiler::mir::MirBoolConst>().value, true);
			ASSERT_EQUAL(false_mir_value.get<compiler::mir::MirBoolConst>().value, false);

			// Don't go into details of the second function. Just validate block IDs.
			auto& goo_mir
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(1) })->value();
			ASSERT_TRUE(goo_mir.validateBlockIDs().isOk());
		});
	}

	void simpleFunctionCalls() {
		auto [module, scope] = getModule(fs::File(path("modules/function_calls")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(3, functions.size());

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(2) })->value();
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));
			ASSERT_TRUE(foo_mir.validateBlockIDs().isOk());

			u64 count_of_calls = 0;

			static std::array functions_to_call = {
				base::StrID("arg1"), base::StrID("arg0"), base::StrID("arg1"),
				base::StrID("arg0"), base::StrID("arg1"), base::StrID("arg1"),
			};

			auto entry_block = foo_mir.block_order[0];
			for (auto& instruction: foo_mir.blocks[entry_block].instructions) {
				if (instruction.operation == compiler::mir::Operation::Call) {
					auto callee
						= instruction.arguments.at(0).get<compiler::mir::MirFunctionLiteral>();
					ASSERT_EQUAL(
						compiler::helios::name(callee.helios_id),
						functions_to_call.at(count_of_calls)
					);
					count_of_calls++;
				}
			}

			ASSERT_EQUAL(count_of_calls, 6);
		});
	}

	void functionParametersTest() {
		auto [module, scope] = getModule(fs::File(path("modules/function_with_parameters")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(1, functions.size());

			auto& foo_mir
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) })->value();
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));

			auto i16_type = ctx.query<QueryIntegralType>(16);
			auto i32_type = ctx.query<QueryIntegralType>(32);
			auto i64_type = ctx.query<QueryIntegralType>(64);

			const auto& locals = foo_mir.local_list;

			bool was_x = false;
			bool was_y = false;
			bool was_z = false;

			for (auto& local: locals) {
				if (local.getName() == "x") {
					ASSERT_TRUE(not was_x);
					ASSERT_EQUAL(local.parameter_index.value(), 0);
					ASSERT_EQUAL(local.type.getType(), i16_type);
					was_x = true;
				} else if (local.getName() == "y") {
					ASSERT_TRUE(not was_y);
					ASSERT_EQUAL(local.parameter_index.value(), 1);
					ASSERT_EQUAL(local.type.getType(), i32_type);
					was_y = true;
				} else if (local.getName() == "z") {
					ASSERT_TRUE(not was_z);
					ASSERT_EQUAL(local.parameter_index.value(), 2);
					ASSERT_EQUAL(local.type.getType(), i64_type);
					was_z = true;
				} else {
					ASSERT_TRUE(local.parameter_index.empty());
				}
			}

			ASSERT_TRUE(was_x and was_y and was_z);

			// check if value used in the function body is indeed the parameter we expect:
			for (auto block_id: foo_mir.block_order) {
				const auto& block          = foo_mir.blocks[block_id];
				auto        validate_value = [&](const compiler::mir::MIRValue& value) {
                    if (auto local = std::get_if<compiler::mir::LocalRef>(&value.getVariant())) {
                        if ((*local)->parameter_index.has_value())
                            ASSERT_EQUAL((*local)->parameter_index.value(), 2);
                    }
				};

				for (const auto& instr: block.instructions) {
					if (instr.operation == compiler::mir::Operation::DestructIf) continue;
					for (const auto& arg: instr.arguments) validate_value(arg);
				}
				for (const auto& arg: block.terminator.arguments) validate_value(arg);
			}
		});
	}

	void functionEndTest() {
		auto [module, scope] = getModule(fs::File(path("modules/function_end_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(4, functions.size());
			ASSERT_EQUAL(functions.at(0).declaration->original_name, base::StrID("missing_return"));
			ASSERT_EQUAL(
				functions.at(1).declaration->original_name, base::StrID("should_add_retvoid")
			);
			ASSERT_EQUAL(functions.at(2).declaration->original_name, base::StrID("unreachable_end"));
			ASSERT_EQUAL(functions.at(3).declaration->original_name, base::StrID("empty"));

			ASSERT_TRUE(ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) })->hasError()
			);

			auto& should_add_retvoid_fun
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(1) })->value();
			should_add_retvoid_fun.validateBlockIDs();
			std::stringstream foo_str;
			should_add_retvoid_fun.debugPrint(foo_str);

			auto& last_block
				= should_add_retvoid_fun.blocks[should_add_retvoid_fun.block_order.back()];
			ASSERT_EQUAL(last_block.terminator.operation, compiler::mir::Operation::ReturnVoid);


			auto& unreachable_end_fun
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(2) })->value();
			unreachable_end_fun.validateBlockIDs();
			unreachable_end_fun.debugPrint(foo_str);
			ASSERT_EQUAL(unreachable_end_fun.block_order.size(), 7);

			auto& empty
				= ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(3) })->value();
			ASSERT_EQUAL(empty.block_order.size(), 1);
		});
	}

	void moveValidation() {
		// @note This test is very fragile and may require hotfixes even after unrelated changes.
		// Proper tests can be written once 'move' is implemented. It should contain usage of 'if',
		// 'else', 'break', 'continue', 'switch' etc..
		auto [module, scope] = getModule(fs::File(path("modules/move_validation")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;

			for (const compiler::helios::HOUTFunction& fun: functions) {
				if (fun.declaration->original_name.str() == "good1") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();

					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);

					mir_rep.blocks[mir_rep.block_order[0]].instructions[2].flags.emplace_back(
						compiler::mir::OperationFlag::Flag::Move, tmp
					);

					ASSERT_TRUE(validateFunction(mir_rep).isOk());
				}

				if (fun.declaration->original_name.str() == "good2") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();


					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);
					compiler::mir::Instruction&   assignment
						= mir_rep.blocks[mir_rep.block_order[2]].instructions[0];

					// If this test fails use the following to find the correct Instruction.
					// mir_rep.debugPrint(std::cerr);
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

					ASSERT_TRUE(validateFunction(mir_rep).isOk());
				}

				if (fun.declaration->original_name.str() == "good3") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();

					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);

					mir_rep.blocks[mir_rep.block_order[2]].instructions[1].flags.emplace_back(
						compiler::mir::OperationFlag::Flag::Move, tmp
					);

					ASSERT_TRUE(validateFunction(mir_rep).isOk());
				}

				if (fun.declaration->original_name.str() == "good4") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();

					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);

					mir_rep.blocks[mir_rep.block_order[1]].instructions[1].flags.emplace_back(
						compiler::mir::OperationFlag::Flag::Move, tmp
					);

					ASSERT_TRUE(validateFunction(mir_rep).isOk());
				}

				if (fun.declaration->original_name.str() == "bad1") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();

					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);

					mir_rep.blocks[mir_rep.block_order[0]].instructions[2].flags.emplace_back(
						compiler::mir::OperationFlag::Flag::Move, tmp
					);

					ASSERT_TRUE(validateFunction(mir_rep).isBad());
				}

				if (fun.declaration->original_name.str() == "bad2") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();

					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);
					mir_rep.blocks[mir_rep.block_order[1]].instructions[0].flags.emplace_back(
						compiler::mir::OperationFlag::Flag::Move, tmp
					);

					ASSERT_TRUE(validateFunction(mir_rep).isBad());
				}

				if (fun.declaration->original_name.str() == "bad3") {
					auto& mir_rep = (compiler::mir::Function&) ctx
					                    .query<compiler::mir::LowerToMirFunction>({ fun })
					                    ->value();

					CRef<compiler::mir::MirLocal> tmp(mir_rep.local_list[2]);
					mir_rep.blocks[mir_rep.block_order[1]].instructions[0].flags.emplace_back(
						compiler::mir::OperationFlag::Flag::Move, tmp
					);

					ASSERT_TRUE(validateFunction(mir_rep).isBad());
				}
			}
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
