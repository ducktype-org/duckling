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

#include <base/extend_cpp/variant_match.hpp>

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
		TESTER_ADD_TEST(listsTest);
		TESTER_ADD_TEST(staticArraysTest);
		TESTER_ADD_TEST(forContinueTargetTest);
		TESTER_ADD_TEST(pointersTest);
		TESTER_ADD_TEST(boxesTest);
		TESTER_ADD_TEST(testErrorLogging);
		TESTER_ADD_TEST(generatedLocalShadowingTest);
	}

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("modules/slices")), "slices" },
			{ fs::FilePath(path("modules/lists")), "lists" },
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

	void listsTest() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("lists");
		withContextDo([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			auto mir_unit = compiler::mir::lowerToMIRUnit(ctx, &unit).valueOrPanic();
			ASSERT_TRUE(!mir_unit.mir_functions.empty());
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

	void forContinueTargetTest() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("static_arrays");
		withContextDo([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			ASSERT_EQUAL(2u, unit.functions.size());
			auto function = compiler::mir::lowerToPreMIRFunction(ctx, unit.functions.at(1));
			base::Optional<BlockID> continue_target;
			for (auto id: function.block_order) {
				const auto& block = function.blocks[id];
				if (!block.debug_name.has_value()) continue;
				if (block.debug_name.value() == base::StrID("continue"))
					continue_target = block.terminator.arguments.at(0).get<BlockID>();
			}
			ASSERT_HAS_VALUE(continue_target);
			const auto& target_block     = function.blocks[continue_target.value()];
			bool        increments_index = false;
			for (const auto& instr: target_block.instructions) {
				if (instr.operation != compiler::mir::Operation::IntegerAdd
				    || !instr.output.has_value())
					continue;
				auto local = instr.output->getBase<compiler::mir::MIRLocalRef>();
				if (local->getName().strView().starts_with("__index")) increments_index = true;
			}
			ASSERT_TRUE(increments_index);
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

	void boxesTest() {
		auto [module, scope] = getModule(fs::File(path("modules/boxes")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			auto& mir_func = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ unit.functions.at(3) })
			                     ->valueOrThrow();

			auto i64_type = getIntegralType(ctx, 64, Signed);

			bool found_alloc_box_int      = false;
			bool found_alloc_box_point    = false;
			bool found_field_access_read  = false;
			bool found_field_access_write = false;
			bool found_by_val_deref       = false;
			bool found_by_ref_passthrough = false;

			using namespace compiler::mir;
			for (const auto& block_id: mir_func.block_order) {
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					const bool is_box_alloc
						= instr.operation == Operation::Call
					   && compiler::helios::name(
							  instr.arguments[0].get<MIRFunctionLiteral>().helios_id
						  ) == base::StrID("boxAlloc");
					if (is_box_alloc) {
						// var b_int: box i32 = 42;
						// var b_point: box Point = Point(10, 20);
						const auto& arg = instr.arguments[1];
						if (arg.isConstant())
							found_alloc_box_int = true;
						else
							found_alloc_box_point = true;
					} else if (instr.operation == Operation::Assign) {
						const auto& out_place = instr.output.value();
						// b_point.y = 99;
						if (out_place.projection_chain.size() == 2) {
							bool is_deref = v_matches(
								out_place.projection_chain[0].storage, MIRPlace::DerefProjection
							);
							if (is_deref
							    && v_matches(
									out_place.projection_chain[1].storage, MIRPlace::FieldProjection
								)) {
								found_field_access_write = true;
							}
						} else {
							// var x: i32 = b_point.x;
							const auto& arg_place = instr.arguments[0].get<MIRPlace>();
							if (arg_place.projection_chain.size() == 2) {
								bool is_deref = v_matches(
									arg_place.projection_chain[0].storage, MIRPlace::DerefProjection
								);
								if (is_deref
								    && v_matches(
										arg_place.projection_chain[1].storage,
										MIRPlace::FieldProjection
									)) {
									found_field_access_read = true;
								}
							}
						}
					} else if (instr.operation == Operation::Call) {
						// by_val(b_point);
						// by_ref(&b_point);
						const auto& callee      = instr.arguments[0].get<MIRFunctionLiteral>();
						const auto  callee_name = compiler::helios::name(callee.helios_id);
						if (callee_name == "by_val") {
							const auto& arg_place = instr.arguments[1].get<MIRPlace>();
							if (arg_place.projection_chain.size() == 1
							    && v_matches(
									arg_place.projection_chain[0].storage, MIRPlace::DerefProjection
								)) {
								found_by_val_deref = true;
							}
						} else if (callee_name == "by_ref") {
							const auto& arg_place = instr.arguments[1].get<MIRPlace>();

							if (arg_place.projection_chain.empty()) found_by_ref_passthrough = true;
						}
					}
				}
			}

			// return b_int;
			const auto& last_block = mir_func.blocks[mir_func.block_order.back()];
			if (last_block.terminator.operation == Operation::ReturnValue) {
				const auto& ret_val = last_block.terminator.arguments[0].get<MIRPlace>();
				ASSERT_EQUAL(ret_val.type.getType(), i64_type);
				ASSERT_TRUE(ret_val.type.getRefKind() == ReferenceKind::Direct);
			}

			ASSERT_TRUE(found_alloc_box_int);
			ASSERT_TRUE(found_alloc_box_point);
			ASSERT_TRUE(found_field_access_read);
			ASSERT_TRUE(found_field_access_write);
			ASSERT_TRUE(found_by_val_deref);
			ASSERT_TRUE(found_by_ref_passthrough);
		});
	}

	/**
	 * @brief `ptrof` lowers to an unconditional `Operation::AddressOf`.
	 *
	 * Unlike `&`, which forwards a `box`/`ref` operand unchanged and only emits an `AddressOf` for
	 * a direct one, `ptrof` takes the address of the place itself in every case. The operand keeps
	 * its projection chain, so `ptrof m[1]` addresses the indexed element.
	 */
	void pointersTest() {
		auto [module, scope] = getModule(fs::File(path("modules/pointers")));

		withContextDo([&](query::Context& ctx) {
			auto& unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

			base::Optional<CRef<compiler::helios::HOUTFunction>> target;
			for (const auto& fn: unit.functions)
				if (fn->declaration->original_name.strView() == "ptr_of") target = fn;
			ASSERT_HAS_VALUE(target);

			auto& mir_func = (compiler::mir::Function&) ctx
			                     .query<compiler::mir::LowerToMIRFunction>({ target.value() })
			                     ->valueOrThrow();

			using namespace compiler::mir;
			usize address_of_count = 0;
			bool  found_indexed    = false;
			for (const auto& block_id: mir_func.block_order)
				for (const auto& instr: mir_func.blocks[block_id].instructions) {
					if (instr.operation != Operation::AddressOf) continue;
					address_of_count++;

					// `ptrof m[1]`: the index projection survives into the addressed place.
					const auto& chain = instr.arguments[0].get<MIRPlace>().projection_chain;
					if (!chain.empty() && v_matches(chain.back().storage, MIRPlace::IndexProjection))
						found_indexed = true;
				}

			// One per `ptrof`, the `box` operand included — `&b` would forward it instead.
			ASSERT_EQUAL(usize(3), address_of_count);
			ASSERT_TRUE(found_indexed);
		});
	}

	/**
	 * @brief A user variable spelled like the generated `for` locals (`__index`, `__len`) must not
	 *        be reported as shadowing them.
	 *
	 * @note Lowered here rather than in `mir_errors_test`, because indexing a static array emits a
	 *       bounds check whose `panic` is a standard-library language primitive.
	 */
	void generatedLocalShadowingTest() {
		// Declared in the loop body and left unused.
		checkUserAndGeneratedLocalShareName(
			R"(fun main() -> i64 = {
    var coll: i64[10];
    var sum: i64 = 0;
    for (x in coll) {
        let __index: i64 = 20;
        sum = sum + x;
    }
    return sum;
})",
			"__index"
		);

		// The same, with the user's variable actually read.
		checkUserAndGeneratedLocalShareName(
			R"(fun main() -> i64 = {
    var coll: i64[10];
    var sum: i64 = 0;
    for (x in coll) {
        let __index: i64 = 20;
        sum = sum + __index;
    }
    return sum;
})",
			"__index"
		);

		// Mirror direction: the generated local shadows a user variable of the enclosing scope,
		// which is read after the loop, where only it is in scope.
		checkUserAndGeneratedLocalShareName(
			R"(fun main() -> i64 = {
    var coll: i64[10];
    var sum: i64 = 0;
    var __index: i64 = 7;
    for (x in coll) {
        sum = sum + x;
    }
    return sum + __index;
})",
			"__index"
		);
	}

	/**
	 * @brief Lowers `module_content`, which has to compile without errors, and asserts that a user
	 *        and a generated local share `name`, so that a test cannot silently stop testing a
	 *        collision.
	 */
	void checkUserAndGeneratedLocalShareName(std::string_view module_content, std::string_view name) {
		namespace test_utils = compiler::mir::test_utils;

		test_utils::checkLoweredModule(
			module_content,
			[&](query::Context&, const compiler::mir::MIRUnit& unit) {
				u64 total     = 0;
				u64 generated = 0;
				for (const auto& local: test_utils::functionOfUnit(unit, "main")->local_list) {
					if (local.helios_id.empty()) continue;
					if (compiler::helios::name(*local.helios_id).strView() != name) continue;

					total++;
					if (not compiler::helios::maybeSymbolPst(*local.helios_id).has_value())
						generated++;
				}

				ASSERT_EQUAL(u64(2), total);
				ASSERT_EQUAL(u64(1), generated);
			}
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
