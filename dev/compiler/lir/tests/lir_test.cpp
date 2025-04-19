/**
 * @file lir_tests.cpp
 * @brief Tests in this file are very bad right now, because MIR
 * is not yet fully implemented and is hard to properly test.
 */

#include <helios/queries.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace tsh;
using namespace compiler::helios::test_utils;
using query::utils::withContextDo;
using namespace compiler;

class LIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// @TODO
		// tests here don't tests much apart from the fact that code compiles
		// and does not throw.
		// This is due to the fact that LIR is in a very early stage of development
		// and will likely change a lot in near future.
		// Add more tests with future LIR changes.

		TESTER_ADD_TEST(noTest);
		TESTER_ADD_TEST(simpleBools);
		TESTER_ADD_TEST(functionCallTest);
		TESTER_ADD_TEST(functionParametersTest);
	}

private:
	struct LirModuleResult {
		frontend::ModuleID module;
		helios::ScopeID    scope;
		base::Map<
			base::StrID,
			std::tuple<CRef<helios::HOUTFunction>, CRef<mir::Function>, CRef<lir::Function>>>
			funcs{};

		[[nodiscard]] CRef<helios::HOUTFunction> houtFunc(std::string_view name) const {
			return std::get<CRef<helios::HOUTFunction>>(funcs.at(base::StrID(name.data())));
		}

		[[nodiscard]] CRef<mir::Function> mirFunc(std::string_view name) const {
			return std::get<CRef<mir::Function>>(funcs.at(base::StrID(name.data())));
		}

		[[nodiscard]] CRef<lir::Function> lirFunc(std::string_view name) const {
			return std::get<CRef<lir::Function>>(funcs.at(base::StrID(name.data())));
		}
	};

	LirModuleResult getLirOfModule(std::string_view module_path) {
		auto [module, scope] = getModule(fs::FilePath(module_path));
		LirModuleResult result{ .module = module, .scope = scope };

		withContextDo([&](query::Context& ctx) {
			auto unit = ctx.query<helios::QueryTopLevelEntities>(module);
			for (const auto& hout_func: unit->functions) {
				auto mir_func = ctx.query<mir::LowerToMirFunction>({ hout_func });
				auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
				assertTrue(
					lir_func->validateBlockOrder().isOk(),
					base::strConcat("Could not validate LIR function ", lir_func->name)
				);
				result.funcs.put(
					hout_func.original_name, std::make_tuple(CRef(&hout_func), mir_func, lir_func)
				);
			}
		});
		return result;
	}

	void noTest() {
		auto module = getLirOfModule(path("modules/simple"));
		ASSERT_EQUAL(1, module.funcs.size());
		auto foo_lir = module.lirFunc("foo");

		withContextDo([&](query::Context& ctx) {
			// this might change in the future:
			ASSERT_EQUAL(foo_lir->local_list.size(), 4);

			for (auto& local: foo_lir->local_list) {
				if (local->helios_id.has_value() and helios::name(local->helios_id.value()) == "a") {
					ASSERT_EQUAL(
						local->layout.getSourceType(),
						ctx.query<tsh::QueryIntegralType>({ 64, true })
					);
				}
				if (local->helios_id.has_value() and helios::name(local->helios_id.value()) == "b") {
					ASSERT_EQUAL(
						local->layout.getSourceType(),
						ctx.query<tsh::QueryIntegralType>({ 32, true })
					);
				}
			}

			// @TODO: more proper tests here

			// Test debug print:
			// Note that doesn't test much other then that the code doesn't crash/throw exceptions.
			std::stringstream foo_str;
			foo_lir->debugPrint(ctx, foo_str);
		});
	}

	void simpleBools() {
		auto module = getLirOfModule(path("modules/booleans"));
		ASSERT_EQUAL(1, module.funcs.size());
		auto foo_lir = module.lirFunc("foo");

		// note: it might change where those branch operations are placed:
		// if this happen just see mir-output of tested module for mir block numbers

		auto true_lir_value  = foo_lir->block_order.at(0)->terminator.arguments.at(0);
		auto false_lir_value = foo_lir->block_order.at(3)->terminator.arguments.at(0);

		ASSERT_EQUAL(true_lir_value.get<bool>(), true);
		ASSERT_EQUAL(false_lir_value.get<bool>(), false);
	}

	void functionCallTest() {
		auto module  = getLirOfModule(path("modules/function_calls"));
		auto foo_lir = module.lirFunc("foo");
		auto foo_mir = module.mirFunc("foo");
		withContextDo([&](query::Context& ctx) {
			foo_lir->debugPrint(ctx, std::cerr);
			foo_mir->debugPrint(std::cerr);
		});
	}

	void functionParametersTest() {
		auto module  = getLirOfModule(path("modules/function_with_parameters"));
		auto foo_lir = module.lirFunc("foo");

		// this is also called by LIR lowering,
		// but we keep it here as a sanity check:
		ASSERT_TRUE(foo_lir->validateParameters().isOk());

		bool was_x = false;
		bool was_y = false;
		bool was_z = false;

		for (auto& local: foo_lir->local_list) {
			if (local->helios_id.has_value() and helios::name(local->helios_id.value()) == "x") {
				ASSERT_TRUE(not was_x);
				ASSERT_EQUAL(local->parameter_index.value(), 0);
				was_x = true;
			} else if (local->helios_id.has_value()
			           and helios::name(local->helios_id.value()) == "y") {
				ASSERT_TRUE(not was_y);
				ASSERT_EQUAL(local->parameter_index.value(), 1);
				was_y = true;
			} else if (local->helios_id.has_value()
			           and helios::name(local->helios_id.value()) == "z") {
				ASSERT_TRUE(not was_z);
				ASSERT_EQUAL(local->parameter_index.value(), 2);
				was_z = true;
			} else {
				ASSERT_TRUE(local->parameter_index.empty());
			}
		}

		ASSERT_TRUE(was_x and was_y and was_z);

		// check if value in return instruction is indeed the parameter we expect:
		u64 return_value_count = 0;
		for (auto& block: foo_lir->block_order) {
			if (block->terminator.operation == compiler::lir::Operation::ReturnValue) {
				auto z_local = block->terminator.arguments.at(0).get<compiler::lir::LocalRef>();
				ASSERT_EQUAL(z_local->parameter_index.value(), 2);
				return_value_count++;
			}
		}

		ASSERT_EQUAL(return_value_count, 1);
	}
};

TESTER_COMMON_MAIN("/compiler/lir/tests/")
