#include "relocator.hpp"

#include "base/except/exceptions.hpp"
#include <base/preproc/for_each.hpp>

#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/core/fast/utils.hpp>

using namespace vm::fast;

namespace {
#define TRANSLATOR_ARGUMENTS(ARG_NAME)                                               \
	[[maybe_unused]] const ProgramBase &                     program,                \
		[[maybe_unused]] const exec::ExecFunctionCollection &exec_functions,         \
		[[maybe_unused]] const exec::ExecFunction &          current_function,       \
		[[maybe_unused]] const reloc::RelocFunction &        current_function_reloc, \
		[[maybe_unused]] const reloc::Instruction &          current_instruction,    \
		[[maybe_unused]] const reloc::arg::ARG_NAME &        reloc_arg

// Forward declare the argument translation functions.
#define HANDLE_ARG_DEF(ARG_NAME) \
	exec::arg::ARG_NAME relocate##ARG_NAME(TRANSLATOR_ARGUMENTS(ARG_NAME));
#include "vm/core/fast/program/instructions/argument_definitions.hpp"
#undef HANDLE_ARG_DEF

// Define the translations
#define TRIVIAL_TRANSLATION(ARG_NAME)                                                         \
	[[maybe_unused]] exec::arg::ARG_NAME relocate##ARG_NAME(TRANSLATOR_ARGUMENTS(ARG_NAME)) { \
		return reloc_arg;                                                                     \
	}

	FOR_EACH(TRIVIAL_TRANSLATION, Immediate, Place8, Place16, Place32, Place64, JumpDestination)
#undef TRIVIAL_TRANSLATION

	/**
	 * @brief Relocates a function argument (ID) by returning a pointer to the corresponding
	 * executable function.
	 */
	[[maybe_unused]] exec::arg::Function relocateFunction(TRANSLATOR_ARGUMENTS(Function)) {
		return &exec_functions[reloc_arg.asInt()];
	}

	[[maybe_unused]] exec::arg::Type relocateType(TRANSLATOR_ARGUMENTS(Type)) {
		return program.types.at(TypeID(reloc_arg.asInt())).get();
	}

	// @TODO: Remove the maybe_unused attributes.

#undef TRANSLATOR_ARGUMENTS
}

/**
 * @brief Relocates an instruction to an executable instruction by translating its
 * arguments.
 */
exec::Instruction relocInstruction(
	const ProgramBase&                  program,
	const exec::ExecFunctionCollection& exec_functions,
	const exec::ExecFunction&           current_function,
	const reloc::RelocFunction&         current_function_reloc,
	const reloc::Instruction&           instruction
) {
	switch (instruction.id) {
#define ARG_TYPE(type, name)         type
#define ARG_NAME(type, name)         name
#define DO_TRANSLATION(INSTR_MEMBER) DO_TRANSLATION_LAST(INSTR_MEMBER),
#define DO_TRANSLATION_LAST(INSTR_MEMBER) \
	CAT(relocate, ARG_TYPE INSTR_MEMBER)( \
		program,                          \
		exec_functions,                   \
		current_function,                 \
		current_function_reloc,           \
		instruction,                      \
		inner_instr.ARG_NAME INSTR_MEMBER \
	)
#define HANDLE_INSTR_ARGS(INSTR_NAME, ...)                                         \
	case InstrID::INSTR_NAME: {                                                    \
		[[maybe_unused]] auto&& inner_instr = instruction.CAT(instr_, INSTR_NAME); \
		return exec::maker::INSTR_NAME(                                            \
			FOR_EACH_CUSTOM_LAST(DO_TRANSLATION, DO_TRANSLATION_LAST, __VA_ARGS__) \
		);                                                                         \
	}
#include "instructions/instruction_definitions.hpp"
#undef HANDLE_INSTR
#undef ARG_TYPE
#undef ARG_NAME
#undef DO_TRANSLATION
#undef DO_TRANSLATION_LAST
	}
	CORE_UNREACHABLE();
}

exec::ExecFunctionCollection exec::linkFunctions(
	const ProgramBase& program, const reloc::RelocFunctionCollection& reloc_functions
) {
	ExecFunctionCollection exec_functions;

	// "Forward declare"
	exec_functions.resize(reloc_functions.size());

	// Relocate the instructions and fill the executable functions with them.
	for (const auto& [reloc_function, func_info]:
	     std::views::zip(reloc_functions, program.functions)) {
		ExecFunction& current_function = exec_functions[func_info.id.asInt()];
		current_function.data
			= reloc_function.data | std::views::transform([&](const reloc::Instruction& instr) {
				  return relocInstruction(
					  program, exec_functions, current_function, reloc_function, instr
				  );
			  })
		    | std::ranges::to<std::vector>();
		current_function.info = &func_info;
	}

	return exec_functions;
}
