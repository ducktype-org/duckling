#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/loader/loader.hpp>

#include <chrono>
#include <thread>

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(lowVMProgramCopyReplaceOpcode); }


private:
	/**
	 * @brief Checks if editing opcodes works correctly.
	 */
	void lowVMProgramCopyReplaceOpcode() {
		vm::loader::Loader          loader;
		CRef<vm::low::LowVMProgram> program = loader.getProgram();
		vm::low::LowVMProgramCopy   program_copy(program);

		assertEqual(program_copy.getOriginalProgram(), program, "Wrong original program");

		loader.loadAndCompile({ { path("breakpoint.dbc") } });
		assertTrue(program->getFunctions().size(), "No functions loaded");

		assertEqual(
			program_copy.getFunctions().size(),
			0,
			"Program copy works too well. Is actual before selfUpdate()"
		);
		program_copy.selfUpdate();
		assertEqual(
			program_copy.getFunctions().size(),
			program->getFunctions().size(),
			"Programs function count mismach"
		);

		u64 main_function_id = -1ull;
		for (auto& [low_func_data, id, name]: program_copy.getFunctions().allData()) {
			if (name == base::StrID("main")) {
				main_function_id = id;
				break;
			}
		}

		assertFalse(main_function_id == -1ull, "Main function not found");

		vm::low::MicroOpcode opcode       = vm::low::MicroOpcode::breakpoint;
		auto                 maybe_opcode = program_copy.replaceOpcode(main_function_id, 0, opcode);

		assertTrue(maybe_opcode.has_value(), "Main does not seem to have any instructions");

		vm::low::MicroOpcode expected_opcode = vm::low::MicroOpcode::stepGil;
		assertEqual(
			maybe_opcode.value(),
			expected_opcode,
			base::strConcat(
				"Main first istruction was not `",
				vm::low::OPCODE_NAMES[static_cast<usize>(expected_opcode)],
				"` but `",
				vm::low::OPCODE_NAMES[static_cast<usize>(maybe_opcode.value())],
				"`"
			)
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
