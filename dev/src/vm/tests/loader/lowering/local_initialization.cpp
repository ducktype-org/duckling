#include <tester/tester.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/loader/compiler/safe/safe_compiler.hpp>
#include <vm/loader/loader.hpp>

/**
 * @brief Checks how local variables are initialized and deinitialized in micro bytecode.
 *
 * A local is initialized without a block, and the block is only created once something needs to
 * refer to the variable through it.
 */
class LocalInitializationTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LocalInitializationTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(plainVariablesGetNoBlock);
		TESTER_ADD_TEST(referencedVariableStillGetsNoBlockUpfront);
		TESTER_ADD_TEST(pointerVariableIsDeinitializedWithDestructors);
		TESTER_ADD_TEST(variantVariableGetsNoBlockUpfront);
		TESTER_ADD_TEST(liveLocalsAreDeinitializedBeforeReturning);
		TESTER_ADD_TEST(reusedSlotDoesNotForceABlock);
		TESTER_ADD_TEST(zeroingIsSpecializedBySize);
		TESTER_ADD_TEST(pointersNestedInAggregatesAreFound);
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

	/**
	 * @brief Number of locals a function initializes.
	 *
	 * Which init a variable gets depends only on its size, so the tests below count all of them
	 * together.
	 */
	usize countInits(base::StrID function_name) {
		return countOpcode(function_name, MicroOpcode::init_imm_type)
		     + countOpcode(function_name, MicroOpcode::init64_imm_type)
		     + countOpcode(function_name, MicroOpcode::init128_imm_type);
	}

	void assertInitCount(base::StrID function_name, usize expected) {
		const usize found = countInits(function_name);
		assertEqual(
			found,
			expected,
			base::strConcat(
				"`", function_name.str(), "` initializes ", found, " locals instead of ", expected
			)
		);
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
		assertInitCount(name, 2);
		assertOpcodeCount(name, MicroOpcode::deinit, 2);
		assertOpcodeCount(name, MicroOpcode::deinitDtor, 0);
	}

	/**
	 * @brief Taking a variable's address does not make its initialization create the block -
	 * that happens when `ref_pptr_bany` runs.
	 */
	void referencedVariableStillGetsNoBlockUpfront() {
		const auto name = base::StrID("referenced");
		assertInitCount(name, 2);
		assertOpcodeCount(name, MicroOpcode::ref_pptr_bany, 1);
	}

	/**
	 * @brief A variable holding pointers has to release the blocks they point at, whether or not
	 * it ever got a block of its own.
	 */
	void pointerVariableIsDeinitializedWithDestructors() {
		const auto name = base::StrID("holds_pointer");
		assertInitCount(name, 2);
		// One for `ptr`, none for `plain` - a structure of plain fields needs no destructors.
		assertOpcodeCount(name, MicroOpcode::deinitDtor, 1);
		assertOpcodeCount(name, MicroOpcode::deinit, 1);
	}

	/**
	 * @brief A variant gets no block upfront either. Its nested blocks can only be reached
	 * through a block place argument, which creates the variant's own block first.
	 */
	void variantVariableGetsNoBlockUpfront() {
		const auto name = base::StrID("owns_nested_blocks");
		assertInitCount(name, 1);
		// A variant leaves its cleanup to its nested block, so it needs no destructors of its own.
		assertOpcodeCount(name, MicroOpcode::deinit, 1);
	}

	/**
	 * @brief A slot index that holds differently typed variables at different points in the
	 * function still initializes all of them without a block - the type is recorded per slot
	 * at runtime, not derived from the slot index.
	 */
	void reusedSlotDoesNotForceABlock() {
		const auto name = base::StrID("reuses_a_slot");
		assertInitCount(name, 3);
	}

	/**
	 * @brief `Type::hasDestructors` recurses into structures and tables, so a pointer held inside
	 * one still gets its scope exit lowered to `deinitDtor`.
	 */
	void pointersNestedInAggregatesAreFound() {
		const auto name = base::StrID("nests_pointers");
		assertInitCount(name, 2);
		assertOpcodeCount(name, MicroOpcode::deinitDtor, 2);
		assertOpcodeCount(name, MicroOpcode::deinit, 0);

		// A variant behind a field brings no destructors with it - whatever its active
		// alternative holds is released through the variant's nested block.
		const auto wraps_variant = base::StrID("nests_a_variant");
		assertInitCount(wraps_variant, 1);
		assertOpcodeCount(wraps_variant, MicroOpcode::deinit, 1);
	}

	/**
	 * @brief The sizes that get a plain store instead of a call to `memset` are picked by the
	 * size of the variable alone, not by what its type is made of.
	 */
	void zeroingIsSpecializedBySize() {
		// `counter` is an `i64`, `flag` an `i8` - only the first has a specialized size.
		const auto mixed_sizes = base::StrID("simple");
		assertOpcodeCount(mixed_sizes, MicroOpcode::init64_imm_type, 1);
		assertOpcodeCount(mixed_sizes, MicroOpcode::init_imm_type, 1);

		// A pointer and a pair of `i64`s are both 16 bytes wide.
		const auto both_16_bytes = base::StrID("holds_pointer");
		assertOpcodeCount(both_16_bytes, MicroOpcode::init128_imm_type, 2);
	}

	/**
	 * @brief `ret` does no cleanup, so the lowering has to pop whatever is still live -
	 * without touching the return values.
	 */
	void liveLocalsAreDeinitializedBeforeReturning() {
		const auto               name    = base::StrID("returns_with_live_locals");
		std::vector<MicroOpcode> opcodes = opcodesOf(name);

		assertOpcodeCount(name, MicroOpcode::ret, 1);
		// `leftover` only, `ret0` belongs to the caller.
		assertOpcodeCount(name, MicroOpcode::deinit, 1);

		const auto ret = std::ranges::find(opcodes, MicroOpcode::ret);
		assertTrue(ret != opcodes.begin(), "`ret` must not be the first instruction");
		assertTrue(
			*std::prev(ret) == MicroOpcode::deinit,
			"the live local must be deinitialized right before `ret`"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/lowering/");
