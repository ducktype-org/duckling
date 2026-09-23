#include "instruction_lowering.hpp"

#include <base/preproc/equal.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/fast/program/ids.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>

#include <optional>

using namespace vm::loader::compiler;
using namespace vm::fast::reloc;

namespace {
	static usize getByteSize(const vm::code::valid_type::TypeSize& size) {
		return static_cast<usize>(size.assumePointerSize(Bytes(8)));
	}

	struct Context final {
		const vm::code::ValidProgram&       high_program;
		const vm::fast::ProgramBase&        program;
		const detail::FunctionStackContext& stack_ctx;
		const vm::fast::FunctionInfo&       func_info;

		vm::code::StackStateID stack_sid;

		base::HashMap<base::StrID, i64> label_ids{};

		auto getStack() const { return stack_ctx.function.local_stack; }
	};

	// Two makeImmediate overloads: one for immediates read from the bytecode and one for raw
	// integers computed during lowering (stack offsets/sizes passed directly to PUSH).
	arg::Immediate makeImmediate(Context&, const vm::opargs::Immediate& value) {
		return value.value;
	}

	arg::Immediate makeImmediate(Context&, const u64 value) { return value; }

	arg::Place64 makePlace64(Context& ctx, const vm::opargs::Place64& place) {
		return getByteSize(ctx.getStack().getByteOffset(ctx.stack_sid, place.var_name).value());
	}

	arg::Function makeFunction(Context& ctx, const vm::opargs::FunctionName& func) {
		return vm::fast::FunctionID(ctx.high_program.functions().idOf(func.function_name).value());
	}

	arg::PlaceAny makePlaceAny(Context& ctx, const vm::opargs::PlaceAny& place) {
		return getByteSize(ctx.getStack().getByteOffset(ctx.stack_sid, place.var_name).value());
	}

	[[maybe_unused]] arg::PlaceAny makePlaceAny(Context&, u64 place_any) { return place_any; }

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

	// Defines a helper `MakerHelper_<NAME>(Context& ctx, auto&& a0, auto&& a1, ...)` whose
	// parameters mirror the instruction's arguments. It runs each raw argument through the matching
	// make<ArgType>(ctx, arg) translator and forwards the results to `reloc::maker::<NAME>(...)`.
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

#include <vm/core/fast/program/instructions/instruction_definitions.def.hpp>
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
		const base::HashMap<i64, usize>&           label_id_to_offset
	) {
		// Now, change label IDs to offsets.
		// These macros detect if an instruction uses a label argument, and if so, replaces the
		// label ID with the offset calculated above.

		auto get_offset = [&](i64 label_id, const vm::fast::reloc::Instruction& instr) -> i64 {
			i64       target_offset  = static_cast<i64>(label_id_to_offset.at(label_id));
			ptrdiff_t current_offset = &instr - new_instructions.data();
			return target_offset - current_offset;
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
		[[maybe_unused]] auto& inner = instr.CAT(instr_, NAME);   \
		FOR_EACH(ADD_LABEL_TRANSLATOR EXPAND, __VA_ARGS__) break; \
	}

		for (vm::fast::reloc::Instruction& instr: new_instructions) {
			switch (instr.id) {
// IF(c)(x) emits x when c is true and nothing otherwise (there is no else branch). COND is true
// only for instructions carrying a JumpDestination argument, so every other instruction is left
// untouched and falls through to the `default` case below.
#define HANDLE_INSTR_ARGS(NAME, ...) IF(COND(__VA_ARGS__))(BODY(NAME, __VA_ARGS__))
#include <vm/core/fast/program/instructions/instruction_definitions.def.hpp>
#undef HANDLE_INSTR_ARGS

			default:
				break;
			}
		}
	}
}

#define PUSH(name, ...)                                                                 \
	do {                                                                                \
		new_instructions.push_back(MakerHelper_##name(ctx __VA_OPT__(, ) __VA_ARGS__)); \
		/* prev = new_instructions.back(); */                                           \
	} while (0)

std::vector<vm::fast::reloc::Instruction> vm::loader::compiler::fast::lowerInstructions(
	const vm::code::ValidProgram&       high_program,
	const vm::fast::ProgramBase&        program,
	const detail::FunctionStackContext& stack_ctx,
	const vm::fast::FunctionInfo&       func_info
) {
	std::vector<vm::fast::reloc::Instruction> new_instructions;

	Context ctx{ .high_program = high_program,
		         .program      = program,
		         .stack_ctx    = stack_ctx,
		         .func_info    = func_info,
		         .stack_sid    = code::StackStateID(0) };

	namespace high = vm::code::instructions;
	base::HashMap<i64, usize> label_id_to_offset;

	base::Optional<Ref<vm::fast::reloc::Instruction>> prev_instr = std::nullopt;
	for (usize i = 0; i < stack_ctx.function.body.size(); i++) {
		const vm::code::Instruction& instruction = stack_ctx.function.body[i];
		ctx.stack_sid                            = stack_ctx.function.stack_states[i];

		if (instruction.opcode() == high::Op_init_pany_type::OPCODE)
			ctx.stack_sid = stack_ctx.function.stack_states.at(i + 1);

		instr_match(instruction) {
			instr_case(high::Op_init_pany_type, init) {
				auto type      = high_program.types().at(init.type.type_name);
				auto type_size = getByteSize(type->getSize());
				PUSH(init_pany_imm, init.var, type_size);
			}
			instr_case(high::Op_deinit, deinit) {
				// Skip, no deinit
			}
			instr_case(high::Op_mov_p64_p64, mov) PUSH(mov_p64_p64, mov.dst, mov.src);
			instr_case(high::Op_mov_p64_imm, mov) PUSH(mov_p64_imm, mov.dst, mov.src);
			instr_case(high::Op_add_p64_p64, add) PUSH(add_p64_p64, add.dst, add.src);
			instr_case(high::Op_add_p64_imm, add) PUSH(add_p64_imm, add.dst, add.src);
			instr_case(high::Op_sub_p64_p64, sub) PUSH(sub_p64_p64, sub.dst, sub.src);
			instr_case(high::Op_sub_p64_imm, sub) PUSH(sub_p64_imm, sub.dst, sub.src);
			instr_case(high::Op_mod_p64_p64, mod) PUSH(mod_p64_p64, mod.dst, mod.src);
			instr_case(high::Op_mod_p64_imm, mod) PUSH(mod_p64_imm, mod.dst, mod.src);
			instr_case(high::Op_call_func, call) {
				const vm::fast::FunctionInfo& called_func_info
					= *program.functions.at(call.function.function_name);
				const usize func_ret_args_size
					= (called_func_info.args_size + called_func_info.return_size).asInt();
				const usize stack_top = getByteSize(ctx.getStack().byteSize(ctx.stack_sid));
				PUSH(call_func_imm, call.function, stack_top - func_ret_args_size);
			}
			instr_case(high::Op_input_p64, input) PUSH(input_p64, input.dst);
			instr_case(high::Op_output_p64, output) PUSH(output_p64, output.src);
			instr_case(high::Op_ret, ret) PUSH(ret_imm, func_info.return_size.asInt());
			instr_case(high::Op_cmpEq_p64_p64, cmp) PUSH(cmpEq_p64_p64, cmp.lhs, cmp.rhs);
			instr_case(high::Op_cmpEq_p64_imm, cmp) PUSH(cmpEq_p64_imm, cmp.lhs, cmp.rhs);
			instr_case(high::Op_cmpGt_p64_p64, cmp) PUSH(cmpGt_p64_p64, cmp.lhs, cmp.rhs);
			instr_case(high::Op_cmpGt_p64_imm, cmp) PUSH(cmpGt_p64_imm, cmp.lhs, cmp.rhs);
			instr_case(high::Op_jmpIf_label, jmp) PUSH(jmpIf_dest, jmp.label);
			instr_case(high::Op_jmpIfNot_label, jmp) PUSH(jmpIfNot_dest, jmp.label);
			instr_case(high::Op_jmp_label, jmp) PUSH(jmp_dest, jmp.label);
			instr_case(high::Op_label, label) {
				const i64 id = makeJumpDestination(ctx, label.label);
				label_id_to_offset.put(id, new_instructions.size());
			}
			instr_default {
				CORE_PANIC(
					"Instruction lowering not implemented for instruction: ", instruction.name()
				);
			}
			// Set prev_instr if prev_instr is not the same as the top instruction.
			// If they are equal then no instructions were added (e.g. label), so we reset
			// prev_instr. if (prev_instr != Ref(&new_instructions.back())) 	prev_instr =
			// &new_instructions.back(); else 	prev_instr = std::nullopt;
		}
	}

	translateLabels(new_instructions, label_id_to_offset);

	return new_instructions;
}
