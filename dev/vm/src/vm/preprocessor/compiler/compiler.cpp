#include "compiler.hpp"

#include <diagnostic/logger.hpp>

#include <base/optional.hpp>
#include <base/stable_type_id_name_map.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/code/code.hpp>
#include <vm/code/instructions.hpp>
#include <vm/code/opcode_args.hpp>
#include <vm/code/serializer/serializer.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/preprocessor/errors.hpp>
#include <vm/preprocessor/parser/elements.hpp>
#include <vm/preprocessor/parser/errors.hpp>
#include <vm/preprocessor/preprocessor.hpp>

#include <expected>
#include <ranges>

namespace vm {
	namespace {
		struct CompContext {
			const base::StableTypeIdNameMap<code::Function, usize>& func_map;
			const TypeMetadata&                                     types;
			PreprocessorLogger&                                     log;
			base::Optional<code::Function>                          func;
			base::Optional<base::HashMap<base::StrID, usize>>       label_positions;
		};

		i64 getOpCodeArgValue(
			const CompContext& ctx, usize instruction_index, const vm::opargs::OpCodeArg& opcode_arg
		) {
			variant_match(opcode_arg) {
				variant_case(vm::opargs::Immediate, imm) return imm.value;

#define HANDLE_OFFSET(Type) variant_case(vm::opargs::Type, offset_type) return offset_type.offset;
				FOR_EACH(HANDLE_OFFSET, VM_OPARG_OFFSET_TYPES);
#undef HANDLE_OFFSET

				variant_case(vm::opargs::Type, type_arg) {
					auto type_obj = ctx.types.atMaybe(type_arg.type_name);
					if (type_obj)
						return static_cast<i64>(static_cast<u64>(type_obj.value()->getID()));
					ctx.log.log<vm::preprocessor::UnknownType>(type_arg, type_arg.type_name);
					return 0;
				}
				variant_case(vm::opargs::FunctionName, func) {
					for (i64 i = 0; i < ctx.func_map.size(); i++)
						if (ctx.func_map.at(base::safeIntConv<u64>(i))->name == func.function_name)
							return i;
					ctx.log.log<vm::preprocessor::UnknownFunction>(func, func.function_name);
					return 0;
				}
				variant_case(vm::opargs::Label, label) {
					auto it = ctx.label_positions->find(label.label_name);
					if (it != ctx.label_positions->end()) {
						// We have to calculate the
						// difference instead of absolute jump position,
						// because our instruction counter is a pointer.
						return static_cast<i64>(it->second) - static_cast<i64>(instruction_index)
						     - 1;
					}
					ctx.log.log<vm::preprocessor::UnknownLabel>(label);
					return 0;
				}
			}
			CORE_UNREACHABLE();
		}

		void splitCodeAndLabels(CompContext& ctx) {
			code::Function new_func = ctx.func.value();
			new_func.body.clear();
			base::HashMap<base::StrID, usize>                        label_positions;
			base::HashMap<base::StrID, code::instructions::Op_label> labels;
			for (const auto& instr: ctx.func.value().body) {
				variant_match(instr) {
					variant_case(code::instructions::Comment, _);
					variant_case(code::instructions::Op_label, label) {
						auto [_, inserted] = label_positions.insert_or_assign(
							label.arg0.label_name, new_func.body.size()
						);
						if (!inserted) {
							ctx.log.logMap<vm::preprocessor::RepeatedLabel>(label, [&](auto& msg) {
								for (auto&& lbl: label_positions)
									if (lbl.first == label.arg0.label_name)
										msg->addNote(makeBox<vm::preprocessor::RepeatedLabelNote>(
											labels[label.arg0.label_name].bytecode_pos.value()
										));
							});
						} else {
							labels.put(label.arg0.label_name, label);
						}
					}
					variant_default { new_func.body.push_back(instr); }
				}
			}
			ctx.func            = std::move(new_func);
			ctx.label_positions = std::move(label_positions);
		}

		vm::low::FuncData changeFuncToFuncData(const CompContext& ctx) {
			vm::low::FuncData func_data;
			func_data.name             = ctx.func->name;
			func_data.arg_size         = ctx.func->arg_size;
			func_data.local_stack_size = ctx.func->local_stack_size;
			func_data.ret_size         = ctx.func->ret_size;

			for (usize op_idx = 0; op_idx < ctx.func->body.size(); op_idx++) {
				const auto& op    = ctx.func->body[op_idx];
				i64         arg_0 = 0;
				i64         arg_1 = 0;
				variant_match(op) {
#define HANDLE_OPCODE_0ARGS(opcode) \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {}
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)              \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {       \
		arg_0 = getOpCodeArgValue(ctx, op_idx, instr.arg0); \
	}
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)   \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {       \
		arg_0 = getOpCodeArgValue(ctx, op_idx, instr.arg0); \
		arg_1 = getOpCodeArgValue(ctx, op_idx, instr.arg1); \
	}
#include <vm/code/opcodes_list.hpp>
				}

				func_data.bc.emplace_back(vm::Fix8Instruction{
#ifdef USE_TAIL_CALLS
					.opfun = vm::OpFuns::OPFUNS.at(low::fix8FromInstr(op)),
#else
					.opcode = static_cast<u16>(low::fix8FromInstr(op)),
#endif
					.arg0 = static_cast<i32>(arg_0),
					.arg1 = static_cast<i32>(arg_1) });
			}
			return func_data;
		}
	}

	std::expected<low::LowVMProgram, PreprocessorLogger> compiler::compile(const Program& program) {
		std::vector<low::FuncData> converted_functions;
		converted_functions.reserve(program.funcMap().size());
		PreprocessorLogger log;

		Box<TypeMetadata> types = program.produceTypeMetadata();
		auto              ctx   = CompContext{ .func_map        = program.funcMap(),
			                                   .types           = *types,
			                                   .log             = log,
			                                   .func            = {},
			                                   .label_positions = {} };

		for (auto& func: program.funcMap()) {
			code::serialize(func, std::cout);

			ctx.func = func;
			splitCodeAndLabels(ctx);
			auto converted_func = changeFuncToFuncData(ctx);
			converted_functions.push_back(converted_func);
		}

		if (log.bad()) return std::unexpected(std::move(log));
		return low::LowVMProgram{ converted_functions, std::move(types) };
	}
}
