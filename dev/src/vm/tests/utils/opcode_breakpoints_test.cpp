#include <tester/tester.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>

#include <string>

class OpcodeBreakpointsTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OpcodeBreakpointsTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testOpcodeNames);
		TESTER_ADD_TEST(testOpcodeConversions);
	}

	void testOpcodeNames() {
		usize instruction_count = 0;
#define HANDLE_MICRO_INSTR(opcode) ++instruction_count;
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

		ASSERT_EQUAL(instruction_count * 2, vm::low::OP_CASES_COUNT);
		ASSERT_EQUAL(vm::low::OP_CASES_COUNT, vm::low::OPCODE_NAMES.size());

		usize opcode_index = 0;
#define HANDLE_MICRO_INSTR(opcode) ASSERT_EQUAL(#opcode, vm::low::OPCODE_NAMES.at(opcode_index++));
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

#define HANDLE_MICRO_INSTR(opcode) \
	ASSERT_EQUAL(std::string("break_") + #opcode, vm::low::OPCODE_NAMES.at(opcode_index++));
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR

		ASSERT_EQUAL(vm::low::OP_CASES_COUNT, opcode_index);
	}

	void testOpcodeConversions() {
#define HANDLE_MICRO_INSTR(opcode)                                                                 \
	{                                                                                              \
		auto instruction = vm::makeLowInstruction(vm::low::MicroOpcode::opcode);                   \
		vm::setBreakpoint(instruction, true);                                                      \
		ASSERT_TRUE(vm::low::isBreakpoint(vm::getInstructionOpcode(instruction)));                 \
		ASSERT_EQUAL(vm::low::MicroOpcode::break_##opcode, vm::getInstructionOpcode(instruction)); \
		ASSERT_EQUAL(                                                                              \
			vm::low::MicroOpcode::opcode,                                                          \
			vm::low::getUnderlying(vm::getInstructionOpcode(instruction))                          \
		);                                                                                         \
		vm::setBreakpoint(instruction, false);                                                     \
		ASSERT_TRUE(!vm::low::isBreakpoint(vm::getInstructionOpcode(instruction)));                \
		ASSERT_EQUAL(vm::low::MicroOpcode::opcode, vm::getInstructionOpcode(instruction));         \
	}
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/utils/");
