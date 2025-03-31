/**
 * @file mir_tests.cpp
 */

#include <query_framework/query_entry_point.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <tester/tester.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/queries.hpp>

#include <mir/mir_lowering/mir_lowering.hpp>

using namespace tsh;
using namespace compiler::helios::test_utils;
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
	}

private:
	void simpleTest() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_simple_test")));

		withContextDo([&](query::Context& ctx) {
			auto unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module);

			auto& functions = unit->functions;
			ASSERT_EQUAL(4, functions.size());
			ASSERT_EQUAL(base::StrID("foo1"), functions.at(0).original_name);
			ASSERT_EQUAL(base::StrID("foo2"), functions.at(1).original_name);
			ASSERT_EQUAL(base::StrID("foo3"), functions.at(2).original_name);
			ASSERT_EQUAL(base::StrID("foo4"), functions.at(3).original_name);

			auto foo1_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(0));
			auto foo2_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(1));
			auto foo3_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(2));
			auto foo4_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(3));

			ASSERT_EQUAL(foo1_mir.name, base::StrID("foo1"));
			ASSERT_EQUAL(foo2_mir.name, base::StrID("foo2"));
			ASSERT_EQUAL(foo3_mir.name, base::StrID("foo3"));
			ASSERT_EQUAL(foo4_mir.name, base::StrID("foo4"));

			ASSERT_EQUAL(foo1_mir.blocks.size(), 1);
			ASSERT_EQUAL(foo2_mir.blocks.size(), 2);
			ASSERT_EQUAL(foo3_mir.blocks.size(), 6);
			ASSERT_EQUAL(foo4_mir.blocks.size(), 2);

			// This doesn't test much other then that the code doesn't crash/throw exceptions.
			// It also make debug_prints covered by tests.
			std::stringstream all_functions;
			foo1_mir.debugPrint(all_functions);
			foo2_mir.debugPrint(all_functions);
			foo3_mir.debugPrint(all_functions);
			foo4_mir.debugPrint(all_functions);
		});
	}

	void simpleVarTest() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(1, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).original_name);

			auto foo_mir = compiler::mir::lowerToPreMirFunction(ctx, functions.at(0));
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));

			// Test locals:
			ASSERT_EQUAL(foo_mir.local_list.size(), 2);

			auto i32_type = ctx.query<QueryIntegralType>(32);

			{
				auto a = foo_mir.local_list.getCRef(0).value();
				ASSERT_EQUAL(a->getName(), "a");
				ASSERT_EQUAL(a->type.getType(), i32_type);
			}
			{
				auto b = foo_mir.local_list.getCRef(1).value();
				ASSERT_EQUAL(b->getName(), "b");
				ASSERT_EQUAL(b->type.getType(), i32_type);
			}

			// Test code generation:

			ASSERT_EQUAL(foo_mir.blocks.size(), 5);

			// @note: block order is reversed:
			// @note: instruction count does not include terminator instruction:

			using enum compiler::mir::Operation;

			ASSERT_EQUAL(foo_mir.blocks.at(4).id, foo_mir.entry_block);
			ASSERT_EQUAL(foo_mir.blocks.at(4).instructions.size(), 1);
			ASSERT_EQUAL(foo_mir.blocks.at(4).instructions.at(0).operation, Assign);
			ASSERT_EQUAL(foo_mir.blocks.at(4).terminator.operation, Branch);

			ASSERT_EQUAL(foo_mir.blocks.at(3).instructions.size(), 1);
			ASSERT_EQUAL(foo_mir.blocks.at(3).instructions.at(0).operation, Assign);
			ASSERT_EQUAL(foo_mir.blocks.at(3).terminator.operation, Jump);


			// Test debug print:
			// Note that doesn't test much other then that the code doesn't crash/throw exceptions.
			std::stringstream foo_str;
			foo_mir.debugPrint(foo_str);
		});
	}

	void testTerminatorSuccessors() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(1, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).original_name);

			auto foo_mir = ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) });

			ASSERT_EQUAL(foo_mir->name, base::StrID("foo"));
			ASSERT_EQUAL(foo_mir->blocks.size(), 5);
			ASSERT_EQUAL(foo_mir->local_list.size(), 2);

			auto get_block_terminator
				= [&](u64 block_id) { return foo_mir->blocks.at(block_id).terminator; };
			auto get_block_successors = [&](u64 block_id) {
				return getTerminatorSuccessors(get_block_terminator(block_id));
			};

			using compiler::mir::BlockID;
			using BlockList = std::vector<BlockID>;
			ASSERT_EQUAL(get_block_successors(0), BlockList{});
			ASSERT_EQUAL(get_block_successors(1), BlockList{});
			ASSERT_EQUAL(get_block_successors(2), BlockList{ BlockID{ 1 } });
			ASSERT_EQUAL(get_block_successors(3), BlockList{ BlockID{ 1 } });

			// Here, the order does not matter.
			// If it breaks because the order changes,
			// the check has to be changed to an order-free assertion.
			ASSERT_EQUAL(get_block_successors(4), BlockList{ BlockID{ 3 } COMMA BlockID{ 2 } });
		});
	}

	void mockLifetimeAnalysisTest() {
		// since lifetime analysis is a mock implementation, we don't
		// yet test them with much effort.
		// @TODO: add better tests once proper lifetimes implementation is in place
		// But we do want to make sure, that it compiles and does not throw:

		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(1, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).original_name);

			auto foo_mir = ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) });

			ASSERT_EQUAL(foo_mir->name, base::StrID("foo"));
			ASSERT_EQUAL(foo_mir->local_list.size(), 2);
		});
	}

	void simpleBools() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/booleans")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(1, functions.size());

			auto foo_mir = ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) });

			// note: it might change where those branch operations are placed:
			// if this happen just see mir-output of tested module for mir block numbers
			auto true_mir_value  = foo_mir->blocks.at(6).terminator.arguments.at(0);
			auto false_mir_value = foo_mir->blocks.at(3).terminator.arguments.at(0);

			ASSERT_EQUAL(true_mir_value.get<compiler::mir::MirBoolConst>().value, true);
			ASSERT_EQUAL(false_mir_value.get<compiler::mir::MirBoolConst>().value, false);
		});
	}

	void simpleFunctionCalls() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/function_calls")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(3, functions.size());

			auto foo_mir = ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(2) });
			ASSERT_EQUAL(foo_mir->name, base::StrID("foo"));

			u64 count_of_calls = 0;

			static std::array functions_to_call = {
				base::StrID("arg1"), base::StrID("arg0"), base::StrID("arg1"),
				base::StrID("arg0"), base::StrID("arg1"), base::StrID("arg1"),
			};

			auto entry_block = foo_mir->entry_block;
			for (auto& instruction: foo_mir->blocks.at(u64(entry_block)).instructions) {
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
		auto [module, scope] = getModule(fs::FilePath(path("modules/function_with_parameters")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit->functions;
			ASSERT_EQUAL(1, functions.size());

			auto foo_mir = ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) });
			ASSERT_EQUAL(foo_mir->name, base::StrID("foo"));

			auto i16_type = ctx.query<QueryIntegralType>(16);
			auto i32_type = ctx.query<QueryIntegralType>(32);
			auto i64_type = ctx.query<QueryIntegralType>(64);

			const auto& locals = foo_mir->local_list;

			bool was_x = false;
			bool was_y = false;
			bool was_z = false;

			for (auto& local: locals) {
				if (local->getName() == "x") {
					ASSERT_TRUE(not was_x);
					ASSERT_EQUAL(local->parameter_index.value(), 0);
					ASSERT_EQUAL(local->type.getType(), i16_type);
					was_x = true;
				} else if (local->getName() == "y") {
					ASSERT_TRUE(not was_y);
					ASSERT_EQUAL(local->parameter_index.value(), 1);
					ASSERT_EQUAL(local->type.getType(), i32_type);
					was_y = true;
				} else if (local->getName() == "z") {
					ASSERT_TRUE(not was_z);
					ASSERT_EQUAL(local->parameter_index.value(), 2);
					ASSERT_EQUAL(local->type.getType(), i64_type);
					was_z = true;
				} else {
					ASSERT_TRUE(local->parameter_index.empty());
				}
			}

			ASSERT_TRUE(was_x and was_y and was_z);

			// check if value in return instruction is indeed the parameter we expect:
			u64 return_value_count = 0;
			for (auto& block: foo_mir->blocks) {
				if (block.terminator.operation == compiler::mir::Operation::ReturnValue) {
					auto z_local = block.terminator.arguments.at(0).get<compiler::mir::LocalRef>();
					ASSERT_EQUAL(z_local->parameter_index.value(), 2);
					return_value_count++;
				}
			}
			ASSERT_EQUAL(return_value_count, 1);
		});
	}
};

TESTER_COMMON_MAIN("/compiler/mir/tests/")
