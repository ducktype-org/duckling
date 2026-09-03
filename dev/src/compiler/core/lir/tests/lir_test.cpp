/**
 * @file lir_tests.cpp
 * @brief Tests in this file are very bad right now, because MIR
 * is not yet fully implemented and is hard to properly test.
 */


#include "utils/lir_test_utils.hpp"

#include <driver/test_utils.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <tsl/queries.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <sstream>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using namespace compiler::lir::test_utils;
using query::utils::withContextDo;
using namespace compiler;

class LIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(noTest);
		TESTER_ADD_TEST(simpleBools);
		TESTER_ADD_TEST(functionParametersTest);
		TESTER_ADD_TEST(testGlobals);
		TESTER_ADD_TEST(testFromFunctionLiterals);
		TESTER_ADD_TEST(testLIRGlobal);
		TESTER_ADD_TEST(testLifetimeFlags);
		TESTER_ADD_TEST(conditionalDestructTest);
		TESTER_ADD_TEST(referencesTest);
		TESTER_ADD_TEST(staticArrayTest);
		TESTER_ADD_TEST(metaFunctionsTest);
		TESTER_ADD_TEST(simpleConstant);
		TESTER_ADD_TEST(cVariadicAbiTest);
		TESTER_ADD_TEST(debugPrintStandaloneElements);
		TESTER_ADD_TEST(debugPrintElementsWithFunctionIDs);
		TESTER_ADD_TEST(debugPrintFunctionWithAndWithoutContext);
		TESTER_ADD_TEST(debugPrintAggregatesWithAndWithoutContext);
	}

protected:
	void beforeAll() override {
		// Initialize the compiler for the STD to load.
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		auto         init_result
			= compiler::driver::test_utils::initializeCompilerForTests({}, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	void noTest() {
		auto module = getLIROfModule(path("modules/simple"));

		ASSERT_EQUAL_PRINT(1, module.funcs.size());
		auto foo_lir = module.lirFunc("foo");

		withContextDo([&](query::Context& ctx) {
			// this might change in the future:

			ASSERT_EQUAL_PRINT(foo_lir->local_list.size(), 3);

			for (auto& local: foo_lir->local_list) {
				if (local.helios_id.has_value() and helios::name(local.helios_id.value()) == "a") {
					ASSERT_EQUAL(
						local.layout->getSourceType().getType(),
						getIntegralType(
							ctx, 64, compiler::tsh::IntegralAbstractType::Signedness::Signed
						)
					);
				}
				if (local.helios_id.has_value() and helios::name(local.helios_id.value()) == "b") {
					ASSERT_EQUAL(
						local.layout->getSourceType().getType(),
						getIntegralType(
							ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
						)
					);
				}
			}

			// @TODO: more proper tests here

			// Test debug print:
			// Note that doesn't test much other then that the code doesn't crash/throw exceptions.
			std::stringstream foo_str;
			foo_lir->debugPrint(foo_str, Ref{ &ctx });
		});
	}

	void simpleBools() {
		auto module = getLIROfModule(path("modules/booleans"));
		ASSERT_EQUAL_PRINT(2, module.funcs.size());
		auto foo_lir = module.lirFunc("foo");

		// note: it might change where those branch operations are placed:
		// if this happens, just see lir-output of tested module for lir block numbers

		auto true_lir_value  = foo_lir->block_order.at(0)->terminator.arguments.at(0);
		auto false_lir_value = foo_lir->block_order.at(3)->terminator.arguments.at(0);

		auto true_lir_constant  = true_lir_value.get<compiler::lir::LIRConstant>().value;
		auto false_lir_constant = false_lir_value.get<compiler::lir::LIRConstant>().value;

		ASSERT_EQUAL(true_lir_constant.get<bool>(), true);
		ASSERT_EQUAL(false_lir_constant.get<bool>(), false);
	}

	void functionParametersTest() {
		auto module  = getLIROfModule(path("modules/function_with_parameters"));
		auto foo_lir = module.lirFunc("foo");

		// this is also called by LIR lowering,
		// but we keep it here as a sanity check:
		ASSERT_TRUE(foo_lir->validateParameters().isOk());

		bool was_x = false;
		bool was_y = false;
		bool was_z = false;

		for (const auto& local: foo_lir->local_list) {
			if (local.helios_id.has_value() and helios::name(local.helios_id.value()) == "x") {
				ASSERT_TRUE(not was_x);
				ASSERT_EQUAL(local.parameter_index.value(), 0);
				was_x = true;
			} else if (local.helios_id.has_value()
			           and helios::name(local.helios_id.value()) == "y") {
				ASSERT_TRUE(not was_y);
				ASSERT_EQUAL(local.parameter_index.value(), 1);
				was_y = true;
			} else if (local.helios_id.has_value()
			           and helios::name(local.helios_id.value()) == "z") {
				ASSERT_TRUE(not was_z);
				ASSERT_EQUAL(local.parameter_index.value(), 2);
				was_z = true;
			} else {
				ASSERT_TRUE(local.parameter_index.empty());
			}
		}

		ASSERT_TRUE(was_x and was_y and was_z);

		// check if value used in the function body is indeed the parameter we expect:
		for (auto& block: foo_lir->block_order) {
			// only parameter of index 2 is ever used:

			auto validate_value = [&](const compiler::lir::LIRValue& value) {
				if (value.isLocal()) {
					if (auto local = value.get<lir::LIRPlace>().getBase<lir::LIRLocalRef>();
					    local->parameter_index.has_value()) {
						ASSERT_EQUAL(local->parameter_index.value(), 2);
					}
				}
			};

			for (auto& instruction: block->instructions)
				for (auto& arg: instruction.arguments) validate_value(arg);
			for (auto& arg: block->terminator.arguments) validate_value(arg);
		}
	}

	void testGlobals() {
		auto module = getLIROfModule(path("modules/globals"));

		// We have 2 lir functions here:
		// We don't count global ctors,
		// but the LIRUnit, apart from the `foo` function also has inserted tuple constructors for
		// global_tuple, so we have 2 functions in total.
		ASSERT_EQUAL_PRINT(2, module.funcs.size());

		auto foo_lir = module.lirFunc("foo");
		auto g_ctor  = module.lirGlobalData("g").getCtorDtorPair().global_ctor.value();

		withContextDo([&](query::Context& ctx) {
			// This might change in the future:

			ASSERT_EQUAL(foo_lir->local_list.size(), 3);
			// The ctor writes the initial value through a pointer to the global, so it holds that
			// pointer in a local.
			ASSERT_EQUAL(g_ctor->local_list.size(), 1);

			// Check local variable 'a'
			bool found_a = false;
			for (const auto& local: foo_lir->local_list) {
				if (local.helios_id.has_value() && helios::name(local.helios_id.value()) == "a") {
					found_a = true;
					ASSERT_EQUAL(
						local.layout->getSourceType().getType(),
						getIntegralType(
							ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
						)
					);
				}
			}
			ASSERT_TRUE(found_a);

			// Check that there is an assignment to a LIRGlobal in the instructions in foo_lir
			bool found_global_assign = false;
			for (const auto& block: foo_lir->blocks) {
				for (const auto& instr: block.instructions) {
					if (instr.operation == lir::Operation::Assign && instr.output.has_value()) {
						if (instr.output.value().isGlobal()) found_global_assign = true;
					}
				}
			}
			ASSERT_TRUE(found_global_assign);

			// The ctor takes the address of the global and writes the initial value through it, so
			// it holds an `AddressOf` on the global and an `Assign` storing through that pointer.
			bool found_global_address_of = false;
			bool found_store_through_ptr = false;
			for (const auto& block: g_ctor->blocks) {
				for (const auto& instr: block.instructions) {
					if (instr.operation == lir::Operation::AddressOf && !instr.arguments.empty()
					    && instr.arguments.at(0).isGlobal())
						found_global_address_of = true;

					if (instr.operation == lir::Operation::Assign && instr.output.has_value()
					    && instr.output.value().hasProjections())
						found_store_through_ptr = true;
				}
			}
			ASSERT_TRUE(found_global_address_of);
			ASSERT_TRUE(found_store_through_ptr);

			// Test debug print:
			std::stringstream foo_str;
			foo_lir->debugPrint(foo_str, Ref{ &ctx });
		});
	}

	void testFromFunctionLiterals() {
		auto module = getLIROfModule(path("modules/globals"));
		auto g_ctor = module.lirGlobalData("g").getCtorDtorPair().global_ctor.value();
		auto some_global_ctor
			= module.lirGlobalData("some_global").getCtorDtorPair().global_ctor.value();
		auto global_tuple_ctor
			= module.lirGlobalData("global_tuple").getCtorDtorPair().global_ctor.value();

		withContextDo([&](query::Context& ctx) {
			std::stringstream foo_str;
			lir::createFunctionInvoker(
				ctx,
				{ g_ctor, some_global_ctor, global_tuple_ctor },
				base::StrID("_MODULE_CTOR_globals")
			)
				.debugPrint(foo_str, Ref{ &ctx });
		});
	}

	void testLIRGlobal() {
		auto module = getLIROfModule(path("modules/globals"));

		ASSERT_EQUAL(3, module.globals.size());

		auto g            = module.lirGlobalData("g");
		auto some_global  = module.lirGlobalData("some_global");
		auto global_tuple = module.lirGlobalData("global_tuple");

		ASSERT_EQUAL(lir::LIRGlobalType::Variable, global_tuple.global.type);
		ASSERT_EQUAL(lir::LIRGlobalType::Variable, some_global.global.type);
		ASSERT_EQUAL(lir::LIRGlobalType::Variable, g.global.type);

		// Adjust those checks if we will have CTV initializers for globals in the future.
		ASSERT_HAS_VALUE(g.getCtorDtorPair().global_ctor);
		ASSERT_HAS_VALUE(some_global.getCtorDtorPair().global_ctor);
		ASSERT_HAS_VALUE(global_tuple.getCtorDtorPair().global_ctor);

		// All the globals here are trivially destructible, so none of them gets a dtor.
		ASSERT_TRUE(g.getCtorDtorPair().global_dtor.empty());
		ASSERT_TRUE(some_global.getCtorDtorPair().global_dtor.empty());
		ASSERT_TRUE(global_tuple.getCtorDtorPair().global_dtor.empty());
	}

	void testLifetimeFlags() {
		// @TODO #1262: this test doesn't make much sense yet, add proper tests when classes and
		// composite types such as variants are fully added.
		auto module = getLIROfModule(path("modules/lifetime_flags"));
		ASSERT_EQUAL(3, module.globals.size());
		ASSERT_EQUAL(3, module.lir_unit.lir_globals.size());

		auto my_int   = module.houtGlobal("my_int");
		auto my_bool  = module.houtGlobal("my_bool");
		auto my_float = module.houtGlobal("my_float");

		withContextDo([&](query::Context& ctx) {
			ASSERT_TRUE(my_int->type.isTriviallyDestructible(ctx));
			ASSERT_TRUE(my_bool->type.isTriviallyDestructible(ctx));
			ASSERT_TRUE(my_float->type.isTriviallyDestructible(ctx));
		});
	}

	void simpleConstant() {
		auto module = getLIROfModule(path("modules/constants"));

		assertTrue(module.globals.size() == 1, "Expected one global data FIB_10");

		auto lir_global = module.lirGlobalData("FIB_10");

		// the main assertions of FIB_10 checks:
		assertTrue(
			lir_global.global.type == lir::LIRGlobalType::Constant,
			"Expected FIB_10 to be a constant"
		);
		assertTrue(
			std::holds_alternative<ctv::CompileTimeValue>(lir_global.data_initialization),
			"Expected FIB_10 to have a CTV initial value"
		);
		auto const_numeric = lir_global.getConstValue().get<numeric_value::NumericValue>();
		auto const_value   = const_numeric->get<i64>();
		ASSERT_EQUAL(const_value, 55);
	}

	void conditionalDestructTest() {
		using namespace compiler::lir;
		auto module   = getLIROfModule(path("modules/conditional_destruct"));
		auto lir_func = module.lirFunc("maybeMove");

		const Instruction* branch_on_flag = nullptr;
		for (const auto& block: lir_func->block_order) {
			const auto& terminator = block->terminator;
			if (terminator.operation != Operation::Branch) continue;

			// The condition of the `if` is a condition temporary, the flag is a plain bool local.
			const auto& condition = terminator.arguments.at(0).get<LIRPlace>();
			if (condition.getBase<LIRLocalRef>()->special_kind == LIRLocalSpecialKind::Normal) {
				ASSERT_TRUE(branch_on_flag == nullptr);
				branch_on_flag = &terminator;
			}
		}
		ASSERT_TRUE(branch_on_flag != nullptr);

		const auto flag         = branch_on_flag->arguments.at(0).get<LIRPlace>();
		const auto flag_local   = flag.getBase<LIRLocalRef>();
		const auto destruct     = branch_on_flag->arguments.at(1).get<BlockRef>();
		const auto continuation = branch_on_flag->arguments.at(2).get<BlockRef>();

		withContextDo([&](query::Context& ctx) {
			ASSERT_TRUE(flag.getBaseLayout()->getSourceType().getType().getKind() == Kind::Bool);
			ASSERT_TRUE(flag.getBaseLayout()->getSourceType().isTriviallyDestructible(ctx));
		});

		// The taken side calls the destructor, gives the value up by clearing the flag, and merges
		// back into the continuation.
		bool calls_destructor = false;
		bool clears_flag      = false;
		for (const auto& instr: destruct->instructions) {
			if (instr.operation == Operation::Call) calls_destructor = true;

			if (instr.operation != Operation::Assign or not instr.output.has_value()) continue;
			if (not instr.output->isLocal() or instr.output->getBase<LIRLocalRef>() != flag_local)
				continue;

			auto value = instr.arguments.at(0).get<LIRConstant>().value.get<bool>();
			ASSERT_HAS_VALUE(value);
			ASSERT_TRUE(not value.value());
			clears_flag = true;
		}
		ASSERT_TRUE(calls_destructor);
		ASSERT_TRUE(clears_flag);

		ASSERT_EQUAL(Operation::Jump, destruct->terminator.operation);
		ASSERT_TRUE(destruct->terminator.arguments.at(0).get<BlockRef>() == continuation);

		// A scope may not end inside the conditional block, because the DVM pairs its `init` and
		// `deinit` on a stack. The only scopes ending there are of the locals the destructor call
		// needs itself, and they start there as well.
		auto scope_flags_of = [](const Instruction& instr, ScopeFlag::Flag kind) {
			std::vector<LIRLocalRef> locals;
			for (const auto& scope_flag: instr.scope_flags)
				if (scope_flag.flag == kind) locals.push_back(scope_flag.local);
			return locals;
		};

		std::vector<LIRLocalRef> started;
		std::vector<LIRLocalRef> ended;
		for (const auto& instr: destruct->instructions) {
			for (auto local: scope_flags_of(instr, ScopeFlag::Flag::ScopeStart))
				started.push_back(local);
			for (auto local: scope_flags_of(instr, ScopeFlag::Flag::ScopeEnd))
				ended.push_back(local);
		}
		for (auto local: scope_flags_of(destruct->terminator, ScopeFlag::Flag::ScopeEnd))
			ended.push_back(local);

		ASSERT_EQUAL_PRINT(started.size(), ended.size());
		for (auto local: ended) ASSERT_TRUE(std::ranges::contains(started, local));
	}

	void referencesTest() {
		auto module   = getLIROfModule(path("modules/references"));
		auto lir_func = module.lirFunc("references");

		bool found_simple_address_of     = false;
		bool found_address_of_with_deref = false;
		bool found_complex_assignment    = false;

		using namespace compiler::lir;

		for (const auto& block: lir_func->block_order) {
			for (const auto& instr: block->instructions) {
				if (instr.operation == Operation::AddressOf) {
					ASSERT_EQUAL(instr.arguments.size(), 1);
					auto& arg = instr.arguments[0].get<LIRPlace>();

					if (arg.projection_chain.empty()) {
						found_simple_address_of = true;
					} else if (arg.projection_chain.size() == 2) {
						bool pattern_ok = std::holds_alternative<LIRPlace::DerefProjection>(
											  arg.projection_chain[0].storage
										  )
						               && std::holds_alternative<LIRPlace::FieldProjection>(
											  arg.projection_chain[1].storage
									   );

						if (pattern_ok) {
							auto field = std::get<LIRPlace::FieldProjection>(
								arg.projection_chain[1].storage
							);
							if (helios::name(field.field_id) == base::StrID("x"))
								found_address_of_with_deref = true;
						}
					}
				}

				if (instr.operation == Operation::Assign && instr.output.has_value()) {
					auto& out_place = instr.output.value();

					if (out_place.projection_chain.size() == 5) {
						const auto& chain = out_place.projection_chain;

						bool pattern_ok
							= std::holds_alternative<LIRPlace::DerefProjection>(chain[0].storage)
						   && std::holds_alternative<LIRPlace::FieldProjection>(chain[1].storage)
						   && std::holds_alternative<LIRPlace::DerefProjection>(chain[2].storage)
						   && std::holds_alternative<LIRPlace::FieldProjection>(chain[3].storage)
						   && std::holds_alternative<LIRPlace::DerefProjection>(chain[4].storage);

						if (pattern_ok) {
							auto f_p = std::get<LIRPlace::FieldProjection>(chain[1].storage);
							auto f_x = std::get<LIRPlace::FieldProjection>(chain[3].storage);

							if (helios::name(f_p.field_id) == base::StrID("p")
							    && helios::name(f_x.field_id) == base::StrID("x")) {
								auto constant = instr.arguments[0].get<LIRConstant>();
								auto num
									= constant.value.get<compiler::numeric_value::NumericValue>();
								if (num->get<i32>() == 999) found_complex_assignment = true;
							}
						}
					}
				}
			}
		}

		ASSERT_TRUE(found_simple_address_of);
		ASSERT_TRUE(found_address_of_with_deref);
		ASSERT_TRUE(found_complex_assignment);
	}

	void staticArrayTest() {
		auto module   = getLIROfModule(path("modules/static_arrays"));
		auto lir_func = module.lirFunc("static_array_test");

		using namespace compiler::lir;

		bool found_zero_init_a = false;
		bool found_zero_init_b = false;
		bool found_index_proj  = false;
		bool found_nested_proj = false;

		for (const auto& local: lir_func->local_list) {
			if (!local.helios_id.has_value()) continue;
			auto name = helios::name(local.helios_id.value());

			variant_match(local.layout->getVariant()) {
				variant_case(compiler::tsl::StaticArrayTypeLayout, layout) {
					if (name == "a") {
						// i32[10] -> 10 * 32 = 320
						ASSERT_EQUAL(layout.getSize(), Bits(320));
						ASSERT_EQUAL(layout.getElementCount(), 10);
					}
					if (name == "b") {
						// Point[2] -> 2 * (32+32) = 128
						ASSERT_EQUAL(layout.getSize(), Bits(128));
					}
				}
				variant_default {}
			}
		}

		for (const auto& block: lir_func->block_order) {
			for (const auto& instr: block->instructions) {
				if (instr.operation == Operation::ZeroInitialize) {
					auto& out   = instr.output.value();
					auto  local = out.getBase<LIRLocalRef>();
					if (local->helios_id.has_value()) {
						auto name = helios::name(local->helios_id.value());
						if (name == "a") found_zero_init_a = true;
						if (name == "b") found_zero_init_b = true;
					}
				}

				if (instr.operation == Operation::Assign && instr.output.has_value()) {
					auto& out   = instr.output.value();
					auto  local = out.getBase<LIRLocalRef>();
					if (!local->helios_id.has_value()) continue;
					auto name = helios::name(local->helios_id.value());

					// a[5] -> IndexProjection
					if (name == "a" && out.projection_chain.size() == 1) {
						if (std::holds_alternative<LIRPlace::IndexProjection>(
								out.projection_chain[0].storage
							))
							found_index_proj = true;
					}

					// b[1].y -> Index, Field
					if (name == "b" && out.projection_chain.size() == 2) {
						const auto& chain = out.projection_chain;
						bool        pattern_ok
							= std::holds_alternative<LIRPlace::IndexProjection>(chain[0].storage)
						   && std::holds_alternative<LIRPlace::FieldProjection>(chain[1].storage);

						if (pattern_ok) {
							auto field = std::get<LIRPlace::FieldProjection>(chain[1].storage);
							if (helios::name(field.field_id) == base::StrID("y"))
								found_nested_proj = true;
						}
					}
				}
			}
		}

		ASSERT_TRUE(found_zero_init_a);
		ASSERT_TRUE(found_zero_init_b);
		ASSERT_TRUE(found_index_proj);
		ASSERT_TRUE(found_nested_proj);
	}

	void metaFunctionsTest() {
		auto module = getLIROfModule(path("modules/meta_functions"));

		withContextDo([&](query::Context& ctx) {
			auto meta_type_entity = tsh::getMetaType();
			auto meta_layout      = CRef<tsl::TypeLayout>(
                &ctx.query<tsl::QueryAbstractTypeLayout>(meta_type_entity)->valueOrThrow()
            );

			auto assert_is_meta_local
				= [&](const lir::LIRLocal& local) { ASSERT_EQUAL(*local.layout, *meta_layout); };

			using enum compiler::lir::Operation;
			using MK = compiler::lir::MetaKind;

			// Meta operations are a single `Meta` op parametrized by a `MetaKind` in `extra_params`.
			auto meta_kind = [](const auto& instr) {
				return std::get<compiler::lir::MetaParameters>(instr.extra_params).kind;
			};

			auto check_meta_function = [&](const char* func_name, MK kind, usize arg_size) {
				auto fn = module.lirFunc(func_name);
				for (const auto& local: fn->local_list) assert_is_meta_local(local);
				const auto& block = fn->block_order[0];
				const auto& instr = block->instructions[0];
				ASSERT_TRUE(instr.operation == MetaTypeOperation);
				ASSERT_TRUE(meta_kind(instr) == kind);
				ASSERT_EQUAL(instr.arguments.size(), arg_size);
			};

			check_meta_function("createBox", MK::CreateBox, 1);
			check_meta_function("createRef", MK::CreateRef, 1);
			check_meta_function("createConst", MK::CreateConst, 1);
			check_meta_function("createPtr", MK::CreatePtr, 1);
			check_meta_function("createCPtr", MK::CreateCPtr, 1);
			check_meta_function("createManyPtr", MK::CreateManyPtr, 1);
			check_meta_function("createSlice", MK::CreateSlice, 1);
			check_meta_function("createVariant", MK::CreateVariant, 4);
			check_meta_function("createTuple", MK::CreateTuple, 4);

			{
				auto mega_type = module.lirFunc("megaType");
				for (const auto& local: mega_type->local_list) assert_is_meta_local(local);

				int  create_variant_count = 0;
				int  create_tuple_count   = 0;
				bool call_found           = false;
				for (const auto& instr: mega_type->block_order[0]->instructions)
					if (instr.operation == MetaTypeOperation && meta_kind(instr) == MK::CreateTuple)
						create_tuple_count++;
					else if (instr.operation == MetaTypeOperation
					         && meta_kind(instr) == MK::CreateVariant)
						create_variant_count++;
					else if (instr.operation == Call)
						call_found = true;
				ASSERT_EQUAL(create_tuple_count, 3);
				ASSERT_EQUAL(create_variant_count, 1);
				ASSERT_TRUE(call_found);
			}
		});
	}

	void numericLiteralsTest() {
		auto module        = getLIROfModule(path("modules/literals"));
		auto proc_data_lir = module.lirFunc("foo");

		withContextDo([&](query::Context& ctx) {
			bool found_is_large   = false;
			bool found_result_f32 = false;
			bool found_some_i16   = false;

			auto bool_layout = CRef<tsl::TypeLayout>(
				&ctx.query<tsl::QueryAbstractTypeLayout>(tsh::getBoolType())->valueOrThrow()
			);
			auto f32_layout = CRef<tsl::TypeLayout>(
				&ctx.query<tsl::QueryAbstractTypeLayout>(getFloatType(ctx, 32))->valueOrThrow()
			);
			auto i16_layout = CRef<tsl::TypeLayout>(
				&ctx
					 .query<tsl::QueryAbstractTypeLayout>(getIntegralType(
						 ctx, 16, compiler::tsh::IntegralAbstractType::Signedness::Signed
					 ))
					 ->valueOrThrow()
			);

			for (const auto& local: proc_data_lir->local_list) {
				if (!local.helios_id.has_value()) continue;

				auto name = helios::name(local.helios_id.value());
				if (name == "is_large") {
					ASSERT_EQUAL(local.layout, bool_layout);
					found_is_large = true;
				} else if (name == "result_f32") {
					ASSERT_EQUAL(local.layout, f32_layout);
					found_result_f32 = true;
				} else if (name == "some_i16") {
					ASSERT_EQUAL(local.layout, i16_layout);
					found_some_i16 = true;
				}
			}
			assertTrue(found_is_large, "LIR local 'is_large' was not found");
			assertTrue(found_result_f32, "LIR local 'result_f32' was not found");
			assertTrue(found_some_i16, "LIR local 'some_i16' was not found");

			// Verify that operations were lowered to the correct LIR instructions
			bool found_ugt  = false;
			bool found_fadd = false;
			bool found_sub  = false;

			for (const auto& block: proc_data_lir->blocks) {
				for (const auto& instr: block.instructions)
					if (instr.operation == lir::Operation::IntegerUGt)
						found_ugt = true;
					else if (instr.operation == lir::Operation::FloatAdd)
						found_fadd = true;
					else if (instr.operation == lir::Operation::IntegerSub)
						found_sub = true;
			}

			assertTrue(found_ugt, "LIR instruction 'IntegerUGt' was not found");
			assertTrue(found_fadd, "LIR instruction 'FloatAdd' was not found");
			assertTrue(found_sub, "LIR instruction 'IntegerSub' was not found");
		});
	}

	/**
	 * @brief Tests `@cffi_variadic_fixed_params(n)` on `extern("C")` declarations.
	 */
	void cVariadicAbiTest() {
		auto module     = getLIROfModule(path("modules/c_variadic"));
		auto caller_lir = module.lirFunc("caller");

		using namespace compiler::lir;

		bool found_variadic_call = false;
		for (const auto& block: caller_lir->block_order) {
			for (const auto& instr: block->instructions) {
				if (instr.operation != Operation::Call) continue;

				const auto& literal = instr.arguments.at(0).get<FunctionLiteral>();
				const auto* c_abi   = std::get_if<lir::LIRAbi::CAbi>(&literal.abi.value);
				assertTrue(c_abi != nullptr, "Expected the call to use the C ABI");

				const auto& info = c_abi->function_info;
				ASSERT_EQUAL_PRINT(literal.mangled_name, base::StrID("sum_varargs"));
				ASSERT_EQUAL_PRINT(info.num_fixed_params.value(), 1);
				found_variadic_call = true;
			}
		}
		assertTrue(found_variadic_call, "No call to `sum_varargs` was found in `caller`");

		std::vector<std::pair<std::string_view, helios::SymID>> invalid_declarations;
		for (auto name: { "variadicZeroFixed",
		                  "variadicNoVarArgs",
		                  "variadicUnpromotedFloat",
		                  "variadicUnpromotedInt" })
			invalid_declarations.emplace_back(name, getChain(name, module.scope).back());

		withContextDo([&](query::Context& ctx) {
			for (const auto& [name, symbol]: invalid_declarations)
				assertTrue(
					ctx.query<helios::QuerySymbolABI>(symbol)->hasFailed(),
					base::strConcat("Expected the ABI query to fail for `", name, "`")
				);
		});
	}

	void debugPrintStandaloneElements() {
		auto module  = getLIROfModule(path("modules/simple"));
		auto foo_lir = module.lirFunc("foo");

		lir::LIRPlace output{ foo_lir->local_list[0], {} };
		lir::LIRPlace argument{ foo_lir->local_list[1], {} };

		std::stringstream place_output;
		output.debugPrint(place_output);
		ASSERT_EQUAL(std::string("Local(?0)"), place_output.str());

		std::stringstream value_output;
		lir::LIRValue{ output }.debugPrint(value_output);
		ASSERT_EQUAL(std::string("Local(?0)"), value_output.str());

		lir::Instruction  instruction{ lir::Operation::Assign,
                                      output,
			                           { lir::LIRValue{ argument }, lir::LIRValue{ output } },
			                           {} };
		std::stringstream instruction_output;
		instruction.debugPrint(instruction_output);
		ASSERT_TRUE(instruction_output.str().find("Local(?0) :=") != std::string::npos);
		ASSERT_TRUE(instruction_output.str().find("Assign") != std::string::npos);
		ASSERT_TRUE(instruction_output.str().find("Local(?1), Local(?0)") != std::string::npos);

		std::stringstream block_output;
		lir::LIRValue{ foo_lir->block_order.at(0) }.debugPrint(block_output);
		ASSERT_EQUAL(std::string("Block(?0)"), block_output.str());

		std::stringstream function_output;
		lir::LIRValue{ lir::FunctionLiteral::fromFunction(*foo_lir) }.debugPrint(function_output);
		ASSERT_TRUE(function_output.str().find("Func(") == 0);
		ASSERT_TRUE(
			function_output.str().find(foo_lir->mangled_name.strView()) != std::string::npos
		);

		auto array_module = getLIROfModule(path("modules/static_arrays"));
		auto array_func   = array_module.lirFunc("static_array_test");

		base::Optional<lir::LIRPlace> indexed_place;
		for (const auto block: array_func->block_order) {
			for (const auto& array_instruction: block->instructions) {
				for (const auto& array_argument: array_instruction.arguments) {
					if (!array_argument.is<lir::LIRPlace>()) continue;
					const auto& candidate = array_argument.get<lir::LIRPlace>();
					for (const auto& projection: candidate.projection_chain)
						if (std::holds_alternative<lir::LIRPlace::IndexProjection>(projection.storage
						    ))
							indexed_place.emplace(candidate);
				}
			}
		}

		ASSERT_HAS_VALUE(indexed_place);
		std::stringstream indexed_output;
		indexed_place->debugPrint(indexed_output);
		ASSERT_TRUE(indexed_output.str().find('[') != std::string::npos);
		ASSERT_TRUE(indexed_output.str().find(']') != std::string::npos);
	}

	void debugPrintElementsWithFunctionIDs() {
		auto module  = getLIROfModule(path("modules/simple"));
		auto foo_lir = module.lirFunc("foo");

		// Standalone printing would assign Local(?0) to this place because it is the first
		// encountered local. Function context preserves its actual index in local_list.
		lir::LIRPlace place{ foo_lir->local_list[1], {} };

		std::stringstream place_output;
		place.debugPrint(place_output, foo_lir);
		ASSERT_EQUAL(std::string("Local(1)"), place_output.str());

		std::stringstream value_output;
		lir::LIRValue{ place }.debugPrint(value_output, foo_lir);
		ASSERT_EQUAL(std::string("Local(1)"), value_output.str());

		lir::Instruction  instruction{ lir::Operation::Assign,
                                      place,
			                           { lir::LIRValue{
                                          lir::LIRPlace{ foo_lir->local_list[0], {} } } },
			                           {} };
		std::stringstream instruction_output;
		instruction.debugPrint(instruction_output, foo_lir);
		const auto printed = instruction_output.str();

		ASSERT_TRUE(printed.find("Local(1) :=") != std::string::npos);
		ASSERT_TRUE(printed.find("Local(0)") != std::string::npos);
	}

	void debugPrintFunctionWithAndWithoutContext() {
		auto module  = getLIROfModule(path("modules/simple"));
		auto foo_lir = module.lirFunc("foo");

		std::stringstream function_without_context_output;
		foo_lir->debugPrint(function_without_context_output);
		ASSERT_TRUE(
			function_without_context_output.str().find(
				foo_lir->local_list[0]->layout->toStringIdentification()
			)
			!= std::string::npos
		);
		withContextDo([&](query::Context& ctx) {
			std::stringstream function_with_context_output;
			foo_lir->debugPrint(function_with_context_output, Ref{ &ctx });
			ASSERT_TRUE(
				function_with_context_output.str().find(
					foo_lir->local_list[0]->layout->toStringDefinition(ctx, true, 1)
				)
				!= std::string::npos
			);
		});
	}

	void debugPrintAggregatesWithAndWithoutContext() {
		auto  module      = getLIROfModule(path("modules/globals"));
		auto  global_data = module.lirGlobalData("g");
		auto& global      = global_data.global;

		std::stringstream global_without_context;
		global.debugPrint(global_without_context);
		ASSERT_TRUE(
			global_without_context.str().find(global.layout->toStringIdentification())
			!= std::string::npos
		);

		std::stringstream global_data_without_context;
		global_data.debugPrint(global_data_without_context);
		ASSERT_TRUE(
			global_data_without_context.str().find(global.layout->toStringIdentification())
			!= std::string::npos
		);
		ASSERT_TRUE(
			global_data_without_context.str().find("Data Initialization:") != std::string::npos
		);

		std::stringstream unit_without_context;
		module.lir_unit.debugPrint(unit_without_context);
		ASSERT_TRUE(unit_without_context.str().find("LIRUnit:") != std::string::npos);
		ASSERT_TRUE(unit_without_context.str().find("Globals:") != std::string::npos);
		ASSERT_TRUE(unit_without_context.str().find("Functions:") != std::string::npos);
		ASSERT_TRUE(
			unit_without_context.str().find(global.layout->toStringIdentification())
			!= std::string::npos
		);
		withContextDo([&](query::Context& ctx) {
			const auto definition = global.layout->toStringDefinition(ctx);

			std::stringstream global_with_context;
			global.debugPrint(global_with_context, Ref{ &ctx });
			ASSERT_TRUE(global_with_context.str().find(definition) != std::string::npos);

			std::stringstream global_data_with_context;
			global_data.debugPrint(global_data_with_context, Ref{ &ctx });
			ASSERT_TRUE(global_data_with_context.str().find(definition) != std::string::npos);

			std::stringstream unit_with_context;
			module.lir_unit.debugPrint(unit_with_context, Ref{ &ctx });
			ASSERT_TRUE(unit_with_context.str().find(definition) != std::string::npos);
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/lir/tests/")
