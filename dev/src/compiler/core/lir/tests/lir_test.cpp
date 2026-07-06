/**
 * @file lir_tests.cpp
 * @brief Tests in this file are very bad right now, because MIR
 * is not yet fully implemented and is hard to properly test.
 */


#include <helios/mangler/mangler.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <tsl/queries.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using query::utils::withContextDo;
using namespace compiler;

class LIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(noTest);
		TESTER_ADD_TEST(simpleBools);
		TESTER_ADD_TEST(functionCallTest);
		TESTER_ADD_TEST(functionCallMetadataTest);
		TESTER_ADD_TEST(functionParametersTest);
		TESTER_ADD_TEST(testGlobals);
		TESTER_ADD_TEST(testFromFunctionLiterals);
		TESTER_ADD_TEST(testLIRGlobal);
		TESTER_ADD_TEST(testLifetimeFlags);
		TESTER_ADD_TEST(referencesTest);
		TESTER_ADD_TEST(staticArrayTest);
		TESTER_ADD_TEST(dynamicArrayTest);
		TESTER_ADD_TEST(metaFunctionsTest);
		TESTER_ADD_TEST(simpleConstant);
	}

private:
	/**
	 * @brief Collection of functions compiles to LIR, and their HOUT and MIR counterparts.
	 */
	struct LIRModuleResult final {
		/* * * * * * * * * * *\
		|  HELIOS level data: |
		\* * * * * * * * * * */

		frontend::ModuleID module;
		helios::ScopeID    scope;


		/* * * * * * * * * * *\
		|  LIR level data:    |
		\* * * * * * * * * * */

		lir::LIRUnit lir_unit;

		/* * * * * * * * * * *\
		|  Mapping data:      |
		\* * * * * * * * * * */

		base::Map<base::StrID, std::tuple<CRef<helios::HOUTFunction>, CRef<lir::Function>>> funcs{};
		base::Map<base::StrID, std::tuple<CRef<helios::HOUTGlobalData>, lir::LIRGlobalData>>
			globals{};

		/* * * * * * * * * * *\
		|  Easy api:          |
		\* * * * * * * * * * */

		[[nodiscard]] CRef<helios::HOUTFunction> houtFunc(std::string_view name) const {
			return std::get<CRef<helios::HOUTFunction>>(funcs.at(base::StrID(name)));
		}

		[[nodiscard]] CRef<lir::Function> lirFunc(std::string_view name) const {
			return std::get<CRef<lir::Function>>(funcs.at(base::StrID(name)));
		}

		[[nodiscard]] CRef<helios::HOUTGlobalData> houtGlobal(std::string_view name) const {
			return std::get<CRef<helios::HOUTGlobalData>>(globals.at(base::StrID(name)));
		}

		[[nodiscard]] lir::LIRGlobalData lirGlobalData(std::string_view name) const {
			return std::get<lir::LIRGlobalData>(globals.at(base::StrID(name)));
		}
	};

	/**
	 * Compiles the module at given path to LIR, returning also HOUT counterparts of
	 * functions and globals for easier testing.
	 *
	 * Note that the functions don't include any global constructors/destructors that might be
	 * generated for global variables, but they are included in the LIRGlobalData for the global
	 * variables, so they can be accessed in tests if needed.
	 */
	LIRModuleResult getLIROfModule(std::string_view module_path) {
		auto [module, scope] = getModule(fs::File(module_path));

		base::Optional<LIRModuleResult> result;

		withContextDo([&](query::Context& ctx) {
			helios::HOUTUnit unit = ctx.query<helios::QueryModuleHOUT>(module)->valueOrPanic();

			auto mir_unit = mir::lowerToMIRUnit(ctx, &unit);
			assertTrue(mir_unit.hasValue(), "MIR lowering failed!");

			auto lir_unit = lir::lowerToLIRUnit(ctx, mir_unit.valueOrPanic());

			result.emplace(LIRModuleResult{ .module = module, .scope = scope, .lir_unit = lir_unit }
			);

			// Now we additionally to the lowering also create a mapping from HOUT functions/globals
			// to their LIR counterparts, to easily navigate between those levels in tests. We do it
			// based on mangled names. This should be stable, but beware that if mangling logic or
			// usage changes this might break and require adjustments.

			for (const auto& hout_func: unit.functions) {
				auto mangled_name = helios::mangler::getSimpleMangledName(
					ctx, hout_func->declaration->original_symbol
				);

				// This is O(n^2), but it should be fine in unit tests with small modules.
				for (const auto& lir_func: lir_unit.lir_functions) {
					if (lir_func->mangled_name == mangled_name) {
						result->funcs.put(
							hout_func->declaration->original_name,
							std::make_tuple(hout_func, lir_func)
						);
						break;
					}
				}
				assertTrue(
					result->funcs.contains(base::StrID(hout_func->declaration->original_name)),
					base::strConcat(
						"Failed to find LIR function for HOUT function: ",
						hout_func->declaration->original_name.strView()
					)
				);
			}

			for (const auto& hout_glob: unit.glob_data) {
				auto mangled_name
					= helios::mangler::getSimpleMangledName(ctx, hout_glob->helios_symbol);

				// This is O(n^2), but it should be fine in unit tests with small modules.
				for (const auto& lir_global: lir_unit.lir_globals) {
					if (lir_global.global.mangled_name == mangled_name) {
						result->globals.put(
							hout_glob->original_name, std::make_tuple(hout_glob, lir_global)
						);
						break;
					}
				}
				assertTrue(
					result->globals.contains(base::StrID(hout_glob->original_name)),
					base::strConcat(
						"Failed to find LIR global for HOUT global: ",
						hout_glob->original_name.strView()
					)
				);
			}
		});
		return result.value();
	}

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
			foo_lir->debugPrint(ctx, foo_str);
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

	void functionCallTest() {
		auto module  = getLIROfModule(path("modules/function_calls"));
		auto foo_lir = module.lirFunc("foo");
		withContextDo([&](query::Context& ctx) { foo_lir->debugPrint(ctx, std::cerr); });
	}

/**
 * @brief Helper macro to assert position information with concise syntax.
 * Extracts line/column info from position and validates against expected values.
 */
#define ASSERT_POSITION(                                                              \
	pos, expected_start_line, expected_start_col, expected_end_line, expected_end_col \
)                                                                                     \
	do {                                                                              \
		auto [start_line, start_col] = (pos).getStartLineColumn();                    \
		auto [end_line, end_col]     = (pos).getEndLineColumn();                      \
		ASSERT_EQUAL(start_line, expected_start_line);                                \
		ASSERT_EQUAL(start_col, expected_start_col);                                  \
		ASSERT_EQUAL(end_line, expected_end_line);                                    \
		ASSERT_EQUAL(end_col, expected_end_col);                                      \
	} while (false)

	/**
	 * @brief Test if the stable positions in LIR metadata are correct.
	 * We check if the positions from metadata of the instructions are correct.
	 */
	void functionCallMetadataTest() {
		auto module  = getLIROfModule(path("modules/function_calls"));
		auto foo_lir = module.lirFunc("foo");

		auto print_stable_position
			= [&](const base::Optional<dia_int::StablePosition>& stable, std::string_view label) {
				  if (!stable.has_value()) {
					  std::cerr << "[LIR metadata] " << label << ": <none>\n";
					  return;
				  }

				  auto position = stable.value().getActiveSourcePositionIllegalAccess();
				  auto [start_line, start_column] = position.getStartLineColumn();
				  auto [end_line, end_column]     = position.getEndLineColumn();
				  auto source_start               = position.getStart();
				  auto source_end                 = position.getEnd();

				  std::cerr << "[LIR metadata] " << label << ": " << start_line << ":"
							<< start_column << " -> " << end_line << ":" << end_column << " ["
							<< source_start << ", " << source_end << "]\n";
			  };

		assertTrue(
			foo_lir->metadata.position.has_value(), "Expected function metadata position in LIR foo"
		);
		print_stable_position(foo_lir->metadata.position, "foo.function");
		ASSERT_EQUAL(foo_lir->metadata.source_code_name.value(), base::StrID("foo"));

		assertTrue(!foo_lir->block_order.empty(), "Expected at least one LIR block");
		const auto& block0 = *foo_lir->block_order[0];
		assertTrue(block0.instructions.size() >= 4, "Expected at least 4 instructions in block 0");

		const auto& first_instr  = block0.instructions[0];
		const auto& second_instr = block0.instructions[1];
		const auto& fourth_instr = block0.instructions[3];

		print_stable_position(first_instr.metadata.position, "foo.block_0.instr_0");
		print_stable_position(second_instr.metadata.position, "foo.block_0.instr_1");
		print_stable_position(fourth_instr.metadata.position, "foo.block_0.instr_3");

		auto first_pos
			= first_instr.metadata.position.value().getActiveSourcePositionIllegalAccess();
		auto second_pos
			= second_instr.metadata.position.value().getActiveSourcePositionIllegalAccess();
		auto fourth_pos
			= fourth_instr.metadata.position.value().getActiveSourcePositionIllegalAccess();

		auto [first_start_line, first_start_col] = first_pos.getStartLineColumn();
		auto [first_end_line, first_end_col]     = first_pos.getEndLineColumn();
		ASSERT_EQUAL(first_start_line, 5);
		ASSERT_EQUAL(first_start_col, 5);
		ASSERT_EQUAL(first_end_line, 5);
		ASSERT_EQUAL(first_end_col, 21);

		ASSERT_POSITION(second_pos, 7, 18, 7, 24);
		ASSERT_POSITION(fourth_pos, 7, 5, 7, 30);

		// Test local variable metadata
		bool found_y = false;
		for (const auto& local: foo_lir->local_list) {
			if (!local.helios_id.has_value()) continue;

			auto name = helios::name(local.helios_id.value());
			if (name == base::StrID("y")) {
				found_y = true;
				ASSERT_EQUAL(local.metadata.source_code_name.value(), name);

				// Verify source position matches variable declaration on line 7
				ASSERT_POSITION(
					local.metadata.position.value().getActiveSourcePositionIllegalAccess(), 7, 5, 7, 30
				);
			}
		}
		ASSERT_TRUE(found_y);
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
			ASSERT_EQUAL(g_ctor->local_list.size(), 0);

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

			// Check that there is an assignment to a LIRGlobal in the instructions in g_ctor
			bool found_global_assign_ctor = false;
			for (const auto& block: g_ctor->blocks) {
				for (const auto& instr: block.instructions) {
					if (instr.operation == lir::Operation::Assign && instr.output.has_value()) {
						if (instr.output.value().isGlobal()) found_global_assign_ctor = true;
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
				.debugPrint(ctx, foo_str);
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
	}

	void testLifetimeFlags() {
		// @TODO #1262: this test doesn't make much sense yet, add proper tests when classes and
		// composite types such as variants are fully added.
		auto module = getLIROfModule(path("modules/lifetime_flags"));
		ASSERT_EQUAL(3, module.globals.size());
		ASSERT_EQUAL(3, module.lir_unit.lir_globals.size());

		auto my_int = module.houtGlobal("my_int");
		ASSERT_TRUE(my_int->type.hasNoOpDestructor());

		auto my_bool = module.houtGlobal("my_bool");
		ASSERT_TRUE(my_bool->type.hasNoOpDestructor());

		auto my_float = module.houtGlobal("my_float");
		ASSERT_TRUE(my_float->type.hasNoOpDestructor());
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

	void dynamicArrayTest() {
		auto module   = getLIROfModule(path("modules/dynamic_arrays"));
		auto lir_func = module.lirFunc("dynamic_array_test");

		using namespace compiler::lir;

		bool found_zero_init        = false;
		bool found_push_with_params = false;
		bool found_pop_with_params  = false;
		bool found_len              = false;
		bool found_free             = false;

		for (const auto& block: lir_func->block_order) {
			for (const auto& instr: block->instructions) {
				switch (instr.operation) {
				case Operation::ZeroInitialize:
					found_zero_init = true;
					break;
				case Operation::Call: {
					auto name = instr.arguments.at(0).get<FunctionLiteral>().mangled_name.strView();
					if (name.contains("push"))
						found_push_with_params = true;
					else if (name.contains("pop"))
						found_pop_with_params = true;
					else if (name.contains("length"))
						found_len = true;
					break;
				}
				case Operation::ListFree:
					found_free = true;
					break;
				default:
					break;
				}
			}
		}

		ASSERT_TRUE(found_zero_init);
		ASSERT_TRUE(found_push_with_params);
		ASSERT_TRUE(found_pop_with_params);
		ASSERT_TRUE(found_len);
		ASSERT_TRUE(found_free);
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

			{
				auto create_box = module.lirFunc("createBox");
				ASSERT_TRUE(create_box->validateParameters().isOk());
				for (const auto& local: create_box->local_list) assert_is_meta_local(local);
				const auto& block = create_box->block_order[0];
				ASSERT_TRUE(block->instructions[0].operation == MetaCreateBox);
			}

			{
				auto create_ref = module.lirFunc("createRef");
				ASSERT_TRUE(create_ref->validateParameters().isOk());
				for (const auto& local: create_ref->local_list) assert_is_meta_local(local);
				const auto& block = create_ref->block_order[0];
				ASSERT_TRUE(block->instructions[0].operation == MetaCreateRef);
			}

			{
				auto create_ref = module.lirFunc("createConst");
				ASSERT_TRUE(create_ref->validateParameters().isOk());
				for (const auto& local: create_ref->local_list) assert_is_meta_local(local);
				const auto& block = create_ref->block_order[0];
				ASSERT_TRUE(block->instructions[0].operation == MetaCreateConst);
			}

			{
				auto create_variant = module.lirFunc("createVariant");
				for (const auto& local: create_variant->local_list) assert_is_meta_local(local);
				const auto& block = create_variant->block_order[0];
				ASSERT_TRUE(block->instructions[0].operation == MetaCreateVariant);
				ASSERT_EQUAL(block->instructions[0].arguments.size(), 4);
			}

			{
				auto create_tuple = module.lirFunc("createTuple");
				for (const auto& local: create_tuple->local_list) assert_is_meta_local(local);
				const auto& block = create_tuple->block_order[0];
				ASSERT_TRUE(block->instructions[0].operation == MetaCreateTuple);
				ASSERT_EQUAL(block->instructions[0].arguments.size(), 4);
			}

			{
				auto mega_type = module.lirFunc("megaType");
				for (const auto& local: mega_type->local_list) assert_is_meta_local(local);

				int  create_variant_count = 0;
				int  create_tuple_count   = 0;
				bool call_found           = false;
				for (const auto& instr: mega_type->block_order[0]->instructions)
					if (instr.operation == MetaCreateTuple)
						create_tuple_count++;
					else if (instr.operation == MetaCreateVariant)
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
};

TESTER_COMMON_MAIN("/src/compiler/core/lir/tests/")
