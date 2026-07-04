/**
 * @file mir_tests.cpp
 */

#include "utils/test_utils.hpp"

#include <ctv/ctv.hpp>
#include <driver/test_utils.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <mir/mir_lowering/mir_validation.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include "base/extend_cpp/variant_match.hpp"

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using compiler::mir::BlockID;
using query::utils::withContextDo;

class MIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(sliceTest);
		TESTER_ADD_TEST(dynamicArraysTest);
		TESTER_ADD_TEST(staticArraysTest);
		TESTER_ADD_TEST(testErrorLogging);
	}

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("modules/slices")), "slices" },
			{ fs::FilePath(path("modules/dynamic_arrays")), "dynamic_arrays" },
			{ fs::FilePath(path("modules/static_arrays")), "static_arrays" },
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	void sliceTest() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("slices");
		withContextDo([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			auto mir_unit = compiler::mir::lowerToMIRUnit(ctx, &unit).valueOrPanic();
			// This 4 blocks are from the conditions for the slice access bounds check
			ASSERT_EQUAL_PRINT(mir_unit.mir_functions[0]->block_order.size(), 5);
		});
	}

	void dynamicArraysTest() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("dynamic_arrays");
		withContextDo([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			auto& hout_func = unit.functions.at(0);

			auto& mir_func = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ hout_func })
			                     ->valueOrThrow();

			bool found_zero_init        = false;
			bool found_push             = false;
			bool found_pop              = false;
			bool found_length_call      = false;
			bool found_index_projection = false;

			using namespace compiler::mir;

			for (const auto& block_id: mir_func.block_order) {
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					switch (instr.operation) {
					case Operation::ZeroInitialize: {
						auto& out_place = instr.output.value();
						if (out_place.getBase<MIRLocalRef>()->getName() == "l")
							found_zero_init = true;
						break;
					}
					case Operation::Call: {
						auto callee = instr.arguments.at(0).get<MIRFunctionLiteral>();
						auto name   = compiler::helios::name(callee.helios_id);

						if (name == base::StrID("push"))
							found_push = true;
						else if (name == base::StrID("pop"))
							found_pop = true;
						else if (name == base::StrID("length"))
							found_length_call = true;

						break;
					}
					case Operation::Assign:
					case Operation::Cast: {
						if (instr.output.has_value()) {
							auto& out_place = instr.output.value();
							// `l[0] = 42` lowers to a `Field(ptr)` projection followed by an
							// `Index` projection.
							if (out_place.getBase<MIRLocalRef>()->getName() == "l") {
								for (const auto& proj: out_place.projection_chain)
									if (v_matches(proj.storage, MIRPlace::IndexProjection))
										found_index_projection = true;
							}
						}
						break;
					}
					default:
						break;
					}
				}
			}

			ASSERT_TRUE(found_zero_init);
			ASSERT_TRUE(found_push);
			ASSERT_TRUE(found_pop);
			ASSERT_TRUE(found_length_call);
			ASSERT_TRUE(found_index_projection);
		});
	}

	void staticArraysTest() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("static_arrays");
		withContextDo([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			auto& hout_func = unit.functions.at(0);

			auto& mir_func = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ hout_func })
			                     ->valueOrThrow();

			bool found_zero_init_arr          = false;
			bool found_zero_init_pts          = false;
			bool found_index_projection       = false;
			bool found_complex_pts_projection = false;

			using namespace compiler::mir;

			for (const auto& block_id: mir_func.block_order) {
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					if (instr.operation == Operation::ZeroInitialize) {
						auto& out_place = instr.output.value();
						auto  local     = out_place.getBase<MIRLocalRef>();
						if (local->getName() == "arr") found_zero_init_arr = true;
						if (local->getName() == "pts") found_zero_init_pts = true;
					}

					if ((instr.operation == Operation::Assign
					     || instr.operation == Operation::AddressOf)
					    && instr.output.has_value()) {
						auto& out_place = instr.output.value();
						auto  local     = out_place.getBase<MIRLocalRef>();

						// `arr[2] = ...` is a single `Index` projection.
						if (local->getName() == "arr") {
							for (const auto& proj: out_place.projection_chain)
								if (v_matches(proj.storage, MIRPlace::IndexProjection))
									found_index_projection = true;
						}

						// `pts[0].x = ...` is an `Index` projection followed by a `Field`.
						if (out_place.projection_chain.size() == 2) {
							const auto& chain = out_place.projection_chain;

							bool is_idx = v_matches(chain[0].storage, MIRPlace::IndexProjection);
							bool is_fld = v_matches(chain[1].storage, MIRPlace::FieldProjection);

							if (is_idx && is_fld) {
								auto field = std::get<MIRPlace::FieldProjection>(chain[1].storage);
								if (compiler::helios::name(field.field_id) == base::StrID("x"))
									found_complex_pts_projection = true;
							}
						}
					}
				}
			}

			ASSERT_TRUE(found_zero_init_arr);
			ASSERT_TRUE(found_zero_init_pts);
			ASSERT_TRUE(found_index_projection);
			ASSERT_TRUE(found_complex_pts_projection);
		});
	}

	void testErrorLogging() {
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun shadowedVar() = {
				var arr: i32[2];
                var x = 42;
				for (x in arr) {}
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun shadowedIter() = {
				var arr: i32[2];
				for (x in arr) {
					for (x in arr) {}
				}
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
