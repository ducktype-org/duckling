#include <tester/tester.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/loader/compiler/safe/safe_compiler.hpp>
#include <vm/loader/loader.hpp>

#include <ranges>

/**
 * @brief Checks how local variables are initialized and deinitialized in micro bytecode.
 *
 * A local is initialized without a block, and the block is only created once something needs to
 * refer to the variable through it. The exceptions are variables owning nested blocks of their
 * own, which get theirs right away.
 */
class LocalInitializationTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LocalInitializationTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(plainVariablesGetNoBlock);
		TESTER_ADD_TEST(referencedVariableStillGetsNoBlockUpfront);
		TESTER_ADD_TEST(pointerVariableIsDeinitializedWithDestructors);
		TESTER_ADD_TEST(variantVariableGetsItsBlockUpfront);
		TESTER_ADD_TEST(liveLocalsAreDeinitializedBeforeReturning);
	}

private:
	using MicroOpcode = vm::low::MicroOpcode;

	base::Optional<CRef<vm::low::LowVMProgram>>              program{};
	vm::loader::Loader                                       loader{};
	base::Optional<vm::loader::compiler::safe::SafeCompiler> compiler{};

	CRef<vm::low::LowVMProgram> loadFixture() {
		if (!program) {
			vm::api::ExecutionConfig config{};
			ASSERT_HAS_VALUE(loader.loadAndValidate({ { path("local_initialization.dbc") } }, config)
			);
			compiler.emplace(*loader.getHighProgram());
			compiler->recompile();
			program = compiler->getLowProgram();
		}
		return *program;
	}

	/**
	 * @brief Opcodes of the micro instructions a function was lowered to.
	 */
	std::vector<MicroOpcode> opcodesOf(base::StrID function_name) {
		CRef<vm::low::LowVMProgram> low_program = loadFixture();

		auto function_id = low_program->getFunctions().idOf(function_name);
		assertTrue(
			function_id.has_value(), base::strConcat("`", function_name.str(), "` was not compiled")
		);

		std::vector<MicroOpcode> opcodes;
		for (const auto& instruction: low_program->getFunctions().at(*function_id)->bc)
			opcodes.push_back(vm::getInstructionOpcode(instruction));
		return opcodes;
	}

	usize countOpcode(base::StrID function_name, MicroOpcode opcode) {
		return static_cast<usize>(std::ranges::count(opcodesOf(function_name), opcode));
	}

	void assertOpcodeCount(base::StrID function_name, MicroOpcode opcode, usize expected) {
		const usize found = countOpcode(function_name, opcode);
		assertEqual(
			found,
			expected,
			base::strConcat(
				"`",
				function_name.str(),
				"` was lowered with ",
				found,
				" `",
				vm::low::OPCODE_NAMES.at(static_cast<usize>(opcode)),
				"` instructions instead of ",
				expected
			)
		);
	}

	/**
	 * @brief Locals only ever used through data places never need a block.
	 */
	void plainVariablesGetNoBlock() {
		const auto name = base::StrID("simple");
		assertOpcodeCount(name, MicroOpcode::simpleInit_imm_imm, 2);
		assertOpcodeCount(name, MicroOpcode::initBlock_imm_type, 0);
		assertOpcodeCount(name, MicroOpcode::deinit, 2);
		assertOpcodeCount(name, MicroOpcode::deinit_dtor_imm_type, 0);
	}

	/**
	 * @brief Taking a variable's address does not make its initialization create the block -
	 * that happens when `ref_pptr_bany` runs.
	 */
	void referencedVariableStillGetsNoBlockUpfront() {
		const auto name = base::StrID("referenced");
		assertOpcodeCount(name, MicroOpcode::simpleInit_imm_imm, 2);
		assertOpcodeCount(name, MicroOpcode::initBlock_imm_type, 0);
		assertOpcodeCount(name, MicroOpcode::ref_pptr_bany, 1);
	}

	/**
	 * @brief A variable holding pointers has to release the blocks they point at, whether or not
	 * it ever got a block of its own.
	 */
	void pointerVariableIsDeinitializedWithDestructors() {
		const auto name = base::StrID("holds_pointer");
		assertOpcodeCount(name, MicroOpcode::simpleInit_imm_imm, 2);
		// One for `ptr`, none for `plain` - a structure of plain fields needs no destructors.
		assertOpcodeCount(name, MicroOpcode::deinit_dtor_imm_type, 1);
		assertOpcodeCount(name, MicroOpcode::deinit, 1);
	}

	/**
	 * @brief A variant owns nested blocks, which is much simpler to handle when its own block
	 * exists from the start.
	 */
	void variantVariableGetsItsBlockUpfront() {
		const auto name = base::StrID("owns_nested_blocks");
		assertOpcodeCount(name, MicroOpcode::initBlock_imm_type, 1);
		assertOpcodeCount(name, MicroOpcode::simpleInit_imm_imm, 0);
	}

	/**
	 * @brief `ret_imm` does no cleanup, so the lowering has to pop whatever is still live -
	 * without touching the return values.
	 */
	void liveLocalsAreDeinitializedBeforeReturning() {
		const auto               name    = base::StrID("returns_with_live_locals");
		std::vector<MicroOpcode> opcodes = opcodesOf(name);

		assertOpcodeCount(name, MicroOpcode::ret_imm, 1);
		// `leftover` only, `ret0` belongs to the caller.
		assertOpcodeCount(name, MicroOpcode::deinit, 1);

		const auto ret = std::ranges::find(opcodes, MicroOpcode::ret_imm);
		assertTrue(ret != opcodes.begin(), "`ret_imm` must not be the first instruction");
		assertTrue(
			*std::prev(ret) == MicroOpcode::deinit,
			"the live local must be deinitialized right before `ret_imm`"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/lowering/");
