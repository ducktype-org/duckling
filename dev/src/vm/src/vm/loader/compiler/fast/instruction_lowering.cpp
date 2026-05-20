#include "instruction_lowering.hpp"

#include "base/preproc/for_each.hpp"

#include "vm/bytecode/bytecode.hpp"
#include "vm/bytecode/instructions.hpp"
#include "vm/bytecode/opcode_args.hpp"
#include "vm/core/fast/program/instructions/relocatable.hpp"

using namespace vm::loader::compiler;
using namespace vm::fast::reloc;

namespace {
	static usize getIntTypeSize(const vm::code::valid_type::TypeSize& size) {
		return static_cast<usize>(size.assumePointerSize(Bytes(8)));
	}

	struct Context {
		const vm::code::ValidProgram&       high_program;
		const detail::FunctionStackContext& stack_ctx;

		base::HashMap<base::StrID, i64> label_ids{};
	};

	arg::Immediate makeImmediate(Context&, const vm::opargs::Immediate& value) {
		return value.value;
	}

	arg::Immediate makeImmediate(Context&, const u64 value) { return value; }

	arg::Place64 makePlace64(Context& ctx, const vm::opargs::Place64& place) {
		return getIntTypeSize(ctx.stack_ctx.locals_map.at(place.var_name).offset);
	}

	arg::Function makeFunction(Context& ctx, const vm::opargs::FunctionName& func) {
		return vm::fast::FunctionID(ctx.high_program.functions().idOf(func.function_name).value());
	}

	/**
	 * @brief Converts a label argument into a label ID
	 */
	arg::JumpDestination makeJumpDestination(Context& ctx, const vm::opargs::Label& label) {
		auto [key_val, _] = ctx.label_ids.put(label.label_name, ctx.label_ids.size());
		return key_val->second;
	}

#define PUSH_HELPER_ARG(tp, name)      auto &&name,
#define PUSH_HELPER_ARG_LAST(tp, name) auto&& name
#define MAKE_ARG(tp, name)             CAT(make, tp)(ctx, name),
#define MAKE_ARG_LAST(tp, name)        CAT(make, tp)(ctx, name)

	// Creates a helper to auto-translate parameters to the appropriate types for the instruction
#define HANDLE_INSTR_ARGS(NAME, ...)                                                               \
	[[maybe_unused]] vm::fast::reloc::Instruction CAT(MakerHelper_, NAME)(                         \
		[[maybe_unused]] Context                                                                   \
		& ctx __VA_OPT__(, )                                                                       \
			FOR_EACH_CUSTOM_LAST(PUSH_HELPER_ARG EXPAND, PUSH_HELPER_ARG_LAST EXPAND, __VA_ARGS__) \
	) {                                                                                            \
		return vm::fast::reloc::maker::NAME(                                                       \
			FOR_EACH_CUSTOM_LAST(MAKE_ARG EXPAND, MAKE_ARG_LAST EXPAND, __VA_ARGS__)               \
		);                                                                                         \
	}

#define DEF_INSTR(name, ...) HANDLE_INSTR_ARGS(INSTR_NAME(name, __VA_ARGS__), __VA_ARGS__)

#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
#undef HANDLE_INSTR_ARGS

	/**
	 * @brief Translates the temporary label IDs used during instruction lowering into actual
	 * bytecode offsets. This is a separate step because we do not know the final instruction
	 * offsets until all instructions are generated, and we want to be able to refer to labels
	 * during instruction generation.
	 * @param new_instructions The list of instructions with label IDs as arguments.
	 * @param label_id_to_offset A map from label IDs to their final instruction offsets.
	 */
	void translateLabels(
		std::vector<vm::fast::reloc::Instruction>& new_instructions,
		const base::HashMap<i64, i64>&             label_id_to_offset
	) {
		// Now, change label IDs to offsets.
		// These macros detect if an instruction uses a label argument, and if so, replaces the
		// label ID with the offset calculated above.

		auto get_offset = [&](i64 label_id, const vm::fast::reloc::Instruction& instr) -> i64 {
			ptrdiff_t current_instr_offset = &instr - new_instructions.data();
			i64       label_offset         = label_id_to_offset.at(label_id);
			return label_offset - current_instr_offset;
		};

#define CHECK_GOOD(tp)      EQUAL(JumpDestination, tp),
#define CHECK_GOOD_LAST(tp) EQUAL(JumpDestination, tp)
#define FIRST_PAREN(a, ...) (a)

#define COND(...)                                                                      \
	BITOR_ALL(FOR_EACH_CUSTOM_LAST(                                                    \
		CHECK_GOOD FIRST_PAREN EXPAND, CHECK_GOOD_LAST FIRST_PAREN EXPAND, __VA_ARGS__ \
	))

#define ADD_LABEL_TRANSLATOR(field_tp, field_name) \
	IF(EQUAL(JumpDestination, field_tp))(inner.field_name = get_offset(inner.field_name, instr));

#define BODY(NAME, ...)                                           \
	case vm::fast::InstrID::NAME: {                               \
		auto& inner = instr.CAT(instr_, NAME);                    \
		FOR_EACH(ADD_LABEL_TRANSLATOR EXPAND, __VA_ARGS__) break; \
	}

		for (vm::fast::reloc::Instruction& instr: new_instructions) {
			switch (instr.id) {
#define HANDLE_INSTR_ARGS(NAME, ...) IF(COND(__VA_ARGS__))(BODY(NAME, __VA_ARGS__))
#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
#undef HANDLE_INSTR_ARGS

			default:
				break;
			}
		}
	}
}

#define PUSH(name, ...) \
	new_instructions.push_back(MakerHelper_##name(ctx __VA_OPT__(, ) __VA_ARGS__))

std::vector<vm::fast::reloc::Instruction> vm::loader::compiler::fast::lowerInstructions(
	const vm::code::ValidProgram&       high_program,
	const detail::FunctionStackContext& stack_ctx,
	const vm::fast::FunctionInfo&       func_info
) {
	std::vector<vm::fast::reloc::Instruction> new_instructions;

	Context ctx{
		.high_program = high_program,
		.stack_ctx    = stack_ctx,
	};

	namespace high = vm::code::instructions;
	base::HashMap<i64, i64> label_id_to_offset;
	for (const code::Instruction& instruction: stack_ctx.function.body) {
		instr_match(instruction) {
			instr_case(high::Op_mov_p64_p64, mov) PUSH(mov_p64_p64, mov.dst, mov.src);
			instr_case(high::Op_mov_p64_imm, mov) PUSH(mov_p64_imm, mov.dst, mov.src);
			instr_case(high::Op_add_p64_p64, add) PUSH(add_p64_p64, add.dst, add.src);
			instr_case(high::Op_call_func, call) {
				const Bytes stack_diff = func_info.args_size + func_info.return_size;
				PUSH(call_func_imm, call.function, stack_diff.asInt());
			}
			instr_case(high::Op_input_p64, input) PUSH(input_p64, input.dst);
			instr_case(high::Op_output_p64, output) PUSH(output_p64, output.src);
			instr_case(high::Op_ret, ret) PUSH(ret_imm, func_info.return_size.asInt());
			instr_case(high::Op_cmpEq_p64_p64, cmp) PUSH(cmpEq_p64_p64, cmp.lhs, cmp.rhs);
			instr_case(high::Op_label, label) {
				const i64 id = makeJumpDestination(ctx, label.label);
				label_id_to_offset.put(id, new_instructions.size());
			}
			instr_default {
				CORE_PANIC(
					"Instruction lowering not implemented for instruction: ", instruction.name()
				);
			}
		}
	}

	translateLabels(new_instructions, label_id_to_offset);

	return new_instructions;
}
