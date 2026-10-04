// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lir_builtins_test.cpp
 * @brief LIR tests over the `function_calls` file, which declares the LIR-implemented builtins
 * (`move_in`/`move_out`) locally via `@builtin`. This suite runs WITHOUT the standard library:
 * loading it would inject `core.builtins` through the implicit prelude and clash with the fixture's
 * local declarations.
 */

#include "utils/lir_test_utils.hpp"

#include <helios/symbols/symbol_id.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <diagnostic/module_flags/module_flags.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <iostream>
#include <variant>

using namespace compiler;
using namespace compiler::lir::test_utils;
using query::utils::withContextDo;

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

class LIRBuiltinsTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LIRBuiltinsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(functionCallTest);
		TESTER_ADD_TEST(builtinCallTest);
		TESTER_ADD_TEST(functionCallMetadataTest);
	}

protected:
	// No standard library: the fixture declares its LIR builtins locally, and loading the std would
	// inject `core.builtins` via the implicit prelude, clashing with those declarations.
	void beforeAll() override { dia::configureImmediatePrint(&std::cerr); }

private:
	void functionCallTest() {
		auto module  = getLIROfModule(path("modules/function_calls"));
		auto foo_lir = module.lirFunc("foo");
		withContextDo([&](query::Context& ctx) { foo_lir->debugPrint(std::cerr, Ref{ &ctx }); });
	}

	/**
	 * @brief `move_out` and `move_in` are implemented in LIR: their calls become a read of /
	 * a store into `*pointer` instead of a `Call`.
	 */
	void builtinCallTest() {
		using namespace compiler::lir;

		auto module = getLIROfModule(path("modules/function_calls"));

		auto is_deref = [](const LIRPlace& place) {
			return place.projection_chain.size() == 1
			    && v_matches(place.projection_chain.front().storage, LIRPlace::DerefProjection);
		};

		auto first_instruction = [](CRef<Function> function) {
			return function->block_order.front()->instructions.front();
		};

		// move_in:{i64}(pointer, value) -> `(*pointer) := Assign value`
		auto store = first_instruction(module.lirFunc("writeInto"));
		ASSERT_TRUE(store.operation == Operation::Assign);
		ASSERT_TRUE(is_deref(store.output.value()));

		// move_out:{i64}(pointer) -> `<result> := Assign (*pointer)`
		auto read = first_instruction(module.lirFunc("readOut"));
		ASSERT_TRUE(read.operation == Operation::Assign);
		ASSERT_TRUE(is_deref(read.arguments.at(0).get<LIRPlace>()));
	}

	/**
	 * @brief Test if the stable positions in LIR metadata are correct.
	 * We check if the positions from metadata of the instructions are correct.
	 */
	void functionCallMetadataTest() {
		auto module  = getLIROfModule(path("modules/function_calls"));
		auto foo_lir = module.lirFunc("foo");

		auto print_stable_position
			= [&](const base::Optional<dia::StablePosition>& stable, std::string_view label) {
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

		ASSERT_HAS_VALUE(
			foo_lir->metadata.position, "Expected function metadata position in LIR foo"
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
};

TESTER_COMMON_MAIN("/src/compiler/core/lir/tests/")
