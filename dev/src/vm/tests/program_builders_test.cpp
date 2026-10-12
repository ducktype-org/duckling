// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file dvm_backend_unit_tests.cpp
 */
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>


using namespace vm::code::builders;
using namespace vm::code::instructions;
using vm::code::Instruction;

class DVMBackendUnitTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DVMBackendUnitTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testInstructionBuilder); }


private:
	template<class T>
	void assertInstructionsEqual(Instruction instruction1, T instruction2) {
		ASSERT_EQUAL(instruction1, Instruction{ instruction2 });
	}

	void testInstructionBuilder() {
		assertThrows<base::Panic>(
			[] { auto _ = InstructionBuilder().build(); }, "Built instruction without set kind"
		);

		InstructionBuilder instr_builder;

		// Test `mov_p32_p32`
		instr_builder.setKind(OpKind::mov);
		const auto arg0 = vm::opargs::Place32{ base::StrID("arg0") };
		const auto arg1 = vm::opargs::Place32{ base::StrID("arg1") };
		instr_builder.pushArgs(arg0, arg1);
		Instruction instr = instr_builder.build();
		assertInstructionsEqual(instr, Op_mov_p32_p32{ arg0, arg1 });

		// Test `ret_p32_p32` does not exist
		instr_builder.setKind(vm::code::builders::OpKind::ret);
		assertThrows<base::Panic>(
			[] { auto _ = InstructionBuilder().build(); }, "Built an invalid instruction"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/");
