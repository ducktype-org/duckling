#include <tester/tester.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/loader/compiler/safe/safe_compiler.hpp>
#include <vm/loader/loader.hpp>

#include <ranges>

class StepGilPlacementTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StepGilPlacementTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(stepGilOnlyAtExecutionFlowChanges); }

private:
	/**
	 * @brief The index of the `stepGil` the builder puts at the very start of a function.
	 * With the JIT enabled the function opens with a `check_strategy`, so everything shifts by one.
	 */
#ifdef ENABLE_JIT
	static constexpr usize FUNCTION_ENTRY_STEP_GIL_INDEX = 1;
#else
	static constexpr usize FUNCTION_ENTRY_STEP_GIL_INDEX = 0;
#endif

	/**
	 * @brief The number of `stepGil` instructions expected in `step_gil.dbc`'s `main`:
	 * one for the function entry, one for the `jmpIf_label` and one for the `call_func`.
	 */
	static constexpr usize EXPECTED_STEP_GIL_COUNT = 3;

	/**
	 * @brief Whether an opcode hands the execution over to another place in the program.
	 */
	static bool changesExecutionFlow(vm::low::MicroOpcode opcode) {
		switch (opcode) {
		case vm::low::MicroOpcode::jmp_label:
		case vm::low::MicroOpcode::jmpIf_label:
		case vm::low::MicroOpcode::jmpIfNot_label:
		case vm::low::MicroOpcode::call_func:
		case vm::low::MicroOpcode::call_builtinfunc:
		case vm::low::MicroOpcode::call_cfunc:
		case vm::low::MicroOpcode::call_ffifunc:
		case vm::low::MicroOpcode::virtual_call_pptr_method:
			return true;
		default:
			return false;
		}
	}

	/**
	 * @brief Checks that `stepGil` is lowered only at the start of a function and right in front
	 * of an instruction that changes the execution flow.
	 */
	void stepGilOnlyAtExecutionFlowChanges() {
		vm::loader::Loader                       loader;
		vm::loader::compiler::safe::SafeCompiler compiler(*loader.getHighProgram());
		vm::api::ExecutionConfig                 config{};

		ASSERT_HAS_VALUE(loader.loadAndValidate({ { path("step_gil.dbc") } }, config));
		compiler.recompile();

		CRef<vm::low::LowVMProgram> program = compiler.getLowProgram();
		auto main_function_id               = program->getFunctions().idOf(base::StrID("main"));
		ASSERT_HAS_VALUE(main_function_id);

		const vm::low::MicroBytecode& bc
			= program->getFunctions().at(main_function_id.value())->getBc();

		usize step_gil_count = 0;
		for (const auto& [index, instruction]: std::views::enumerate(bc)) {
			if (vm::getInstructionOpcode(instruction) != vm::low::MicroOpcode::stepGil) continue;

			step_gil_count++;
			if (static_cast<usize>(index) == FUNCTION_ENTRY_STEP_GIL_INDEX) continue;

			const usize next_index = static_cast<usize>(index) + 1;
			assertTrue(
				next_index < bc.size(),
				base::strConcat("`stepGil` at ", index, " is the last instruction of `main`")
			);

			const vm::low::MicroOpcode next_opcode = vm::getInstructionOpcode(bc[next_index]);
			assertTrue(
				changesExecutionFlow(next_opcode),
				base::strConcat(
					"`stepGil` at ",
					index,
					" is followed by `",
					vm::low::OPCODE_NAMES.at(static_cast<usize>(next_opcode)),
					"`, which does not change the execution flow"
				)
			);
		}

		assertEqual(
			step_gil_count,
			EXPECTED_STEP_GIL_COUNT,
			base::strConcat(
				"`main` was lowered with ",
				step_gil_count,
				" `stepGil` instructions instead of ",
				EXPECTED_STEP_GIL_COUNT
			)
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/lowering/");
