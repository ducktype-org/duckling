#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/loader/compiler/safe/safe_compiler.hpp>
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
		vm::loader::Loader                       loader;
		vm::loader::compiler::safe::SafeCompiler compiler(*loader.getHighProgram());
		CRef<vm::low::LowVMProgram>              program = compiler.getLowProgram();
		vm::low::LowVMProgramCopy                program_copy(program);

		ASSERT_EQUAL(program_copy.getOriginalProgram(), program);

		ASSERT_TRUE(loader.loadAndValidate({ { path("breakpoint.dbc") } }).has_value());
		compiler.recompile();

		ASSERT_TRUE(program->getFunctions().size());
		ASSERT_EQUAL_PRINT(program_copy.getFunctions().size(), 0);

		program_copy.selfUpdate();
		ASSERT_EQUAL_PRINT(program_copy.getFunctions().size(), program->getFunctions().size());

		auto main_function_id = program_copy.getFunctions().idOf(base::StrID("main"));
		ASSERT_TRUE(main_function_id.has_value());

		vm::low::MicroOpcode opcode = vm::low::MicroOpcode::breakpoint;
#ifdef ENABLE_JIT
		usize first_opcode_index = 1;
#else
		usize first_opcode_index = 0;
#endif
		auto maybe_opcode
			= program_copy.replaceOpcode(main_function_id.value(), first_opcode_index, opcode);

		ASSERT_TRUE(maybe_opcode.has_value());

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
