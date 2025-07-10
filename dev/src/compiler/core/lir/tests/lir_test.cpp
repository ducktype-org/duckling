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
#include <typesystem/higher/queries/types.hpp>

#include <base/variant.hpp>

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
		TESTER_ADD_TEST(testGlobals);
		TESTER_ADD_TEST(testFromFunctionLiterals);
		TESTER_ADD_TEST(testLirGlobal);
	}

private:
	struct LirModuleResult {
		frontend::ModuleID module;
		helios::ScopeID    scope;
		base::Map<
			base::StrID,
			std::tuple<CRef<helios::HOUTFunction>, CRef<mir::Function>, CRef<lir::Function>>>
			funcs{};
		base::Map<
			base::StrID,
			std::tuple<helios::HOUTGlobalData, CRef<mir::Function>, CRef<lir::Function>>>
			ctors{};

		[[nodiscard]] CRef<helios::HOUTFunction> houtFunc(std::string_view name) const {
			return std::get<CRef<helios::HOUTFunction>>(funcs.at(base::StrID(name.data())));
		}

		[[nodiscard]] CRef<mir::Function> mirFunc(std::string_view name) const {
			return std::get<CRef<mir::Function>>(funcs.at(base::StrID(name.data())));
		}

		[[nodiscard]] CRef<lir::Function> lirFunc(std::string_view name) const {
			return std::get<CRef<lir::Function>>(funcs.at(base::StrID(name.data())));
		}

		[[nodiscard]] helios::HOUTGlobalData houtGlobal(std::string_view name) const {
			return std::get<helios::HOUTGlobalData>(ctors.at(base::StrID(name.data())));
		}

		[[nodiscard]] CRef<mir::Function> mirGlobalCtor(std::string_view name) const {
			return std::get<CRef<mir::Function>>(ctors.at(base::StrID(name.data())));
		}

		[[nodiscard]] CRef<lir::Function> lirGlobalCtor(std::string_view name) const {
			return std::get<CRef<lir::Function>>(ctors.at(base::StrID(name.data())));
		}
	};

	LirModuleResult getLirOfModule(std::string_view module_path) {
		auto [module, scope] = getModule(fs::File(module_path));
		LirModuleResult result{ .module = module, .scope = scope };

		withContextDo([&](query::Context& ctx) {
			auto unit = ctx.query<helios::QueryTopLevelEntities>(module);
			for (const auto& hout_func: unit->functions) {
				CRef mir_func = &ctx.query<mir::LowerToMirFunction>({ hout_func })->value();
				auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
				assertTrue(
					lir_func->validateBlockOrder().isOk(),
					base::strConcat("Could not validate LIR function ", lir_func->mangled_name)
				);
				result.funcs.put(
					hout_func.original_name, std::make_tuple(CRef(&hout_func), mir_func, lir_func)
				);
			}
			for (const auto& hout_glob: unit->glob_data) {
				variant_match(hout_glob.value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_func
							= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_glob })->value();
						auto lir_func = ctx.query<lir::LowerToLirFunction>({ mir_func });
						result.ctors.put(
							hout_glob.original_name, std::make_tuple(hout_glob, mir_func, lir_func)
						);
					}
					variant_case(helios::HOUTGlobalConst, cnst) {
						//@TODO: create global constant ctors if nessesary
						CORE_PANIC(
							"Creating ctors for constant variables is not implemented yet. "
							"Global constant: ",
							hout_glob.original_name.strView()
						);
					}
					variant_default {
						fail(base::strConcat(
							"Unexpected global data type in module: ",
							hout_glob.original_name.strView()
						));
					}
				}
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
						ctx.query<tsh::QueryIntegralType>(
							{ 64, tsh::IntegralAbstractType::Signedness::Signed }
						)
					);
				}
				if (local->helios_id.has_value() and helios::name(local->helios_id.value()) == "b") {
					ASSERT_EQUAL(
						local->layout.getSourceType(),
						ctx.query<tsh::QueryIntegralType>(
							{ 32, tsh::IntegralAbstractType::Signedness::Signed }
						)
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
		ASSERT_EQUAL(2, module.funcs.size());
		auto foo_lir = module.lirFunc("foo");

		// note: it might change where those branch operations are placed:
		// if this happens, just see lir-output of tested module for lir block numbers

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

		// check if value used in the function body is indeed the parameter we expect:
		for (auto& block: foo_lir->block_order) {
			// only parameter of index 2 is ever used:

			auto validate_value = [&](const compiler::lir::LIRValue& value) {
				if (auto local = std::get_if<compiler::lir::LocalRef>(&value.getVariant())) {
					if ((*local)->parameter_index.has_value())
						ASSERT_EQUAL((*local)->parameter_index.value(), 2);
				}
			};

			for (auto& instruction: block->instructions)
				for (auto& arg: instruction.arguments) validate_value(arg);
			for (auto& arg: block->terminator.arguments) validate_value(arg);
		}
	}

	void testGlobals() {
		auto module = getLirOfModule(path("modules/globals"));
		ASSERT_EQUAL(1, module.funcs.size());
		auto foo_lir = module.lirFunc("foo");
		auto g_ctor  = module.lirGlobalCtor("g");

		withContextDo([&](query::Context& ctx) {
			// This might change in the future:

			ASSERT_EQUAL(foo_lir->local_list.size(), 4);
			ASSERT_EQUAL(g_ctor->local_list.size(), 0);

			// Check local variable 'a'
			bool found_a = false;
			for (auto& local: foo_lir->local_list) {
				if (local->helios_id.has_value() && helios::name(local->helios_id.value()) == "a") {
					found_a = true;
					ASSERT_EQUAL(
						local->layout.getSourceType(),
						ctx.query<tsh::QueryIntegralType>(
							{ 64, tsh::IntegralAbstractType::Signedness::Signed }
						)
					);
				}
			}
			ASSERT_TRUE(found_a);

			// Check that there is an assignment to a LirGlobal in the instructions in foo_lir
			bool found_global_assign = false;
			for (const auto& block: foo_lir->blocks) {
				for (const auto& instr: block->instructions) {
					if (instr.operation == lir::Operation::Assign && instr.output.has_value()) {
						if (std::holds_alternative<lir::LirGlobal>(instr.output.value()))
							found_global_assign = true;
					}
				}
			}
			ASSERT_TRUE(found_global_assign);

			// Check that there is an assignment to a LirGlobal in the instructions in g_ctor
			bool found_global_assign_ctor = false;
			for (const auto& block: g_ctor->blocks) {
				for (const auto& instr: block->instructions) {
					if (instr.operation == lir::Operation::Assign && instr.output.has_value()) {
						if (std::holds_alternative<lir::LirGlobal>(instr.output.value()))
							found_global_assign_ctor = true;
					}
				}
			}
			ASSERT_TRUE(found_global_assign_ctor);

			// Test debug print:
			std::stringstream foo_str;
			foo_lir->debugPrint(ctx, foo_str);
		});
	}

	void testFromFunctionLiterals() {
		auto module           = getLirOfModule(path("modules/globals"));
		auto g_ctor           = module.lirGlobalCtor("g");
		auto some_global_ctor = module.lirGlobalCtor("some_global");

		withContextDo([&](query::Context& ctx) {
			std::stringstream foo_str;
			lir::fromLIRFunctions(
				ctx, { g_ctor, some_global_ctor }, base::StrID("_MODULE_CTOR_globals")
			)
				.debugPrint(ctx, foo_str);
		});
	}

	void testLirGlobal() {
		auto module      = getLirOfModule(path("modules/globals"));
		auto g           = module.houtGlobal("g");
		auto some_global = module.houtGlobal("some_global");

		withContextDo([&](query::Context& ctx) {
			auto g_lir           = lir::LirGlobal::fromHOUT(ctx, g);
			auto some_global_lir = lir::LirGlobal::fromHOUT(ctx, some_global);
			ASSERT_EQUAL(lir::LirGlobalType::Variable, some_global_lir.type);
			ASSERT_EQUAL(lir::LirGlobalType::Variable, g_lir.type);
			ASSERT_EQUAL(false, g_lir.inital_value.has_value());
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/lir/tests/")
