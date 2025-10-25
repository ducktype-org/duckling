/**
 * @file dvm_backend_unit_tests.cpp
 */
#include <base/str/string_id.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>

#include <sstream>

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

		// Test `mov_l32_l32`
		instr_builder.setKind(OpKind::mov);
		const auto arg0 = vm::opargs::StackLocal32{ base::StrID("arg0") };
		const auto arg1 = vm::opargs::StackLocal32{ base::StrID("arg1") };
		instr_builder.pushArgs(arg0, arg1);
		Instruction instr = instr_builder.build();
		assertInstructionsEqual(instr, Op_mov_l32_l32{ arg0, arg1 });

		// Test `ret_l32_l32` does not exist
		instr_builder.setKind(vm::code::builders::OpKind::ret);
		assertThrows<base::Panic>(
			[] { auto _ = InstructionBuilder().build(); }, "Built an invalid instruction"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/");
