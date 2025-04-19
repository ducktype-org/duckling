#include "compiler.hpp"

#include <diagnostic/logger.hpp>

#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/process/type_metadata/type.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/loader/parser/errors.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

#include <expected>

namespace vm::loader::compiler {
	namespace {
		struct CompilationContext final {
			const StableTypeIdNameMap<code::Function>& func_map;
			const TypeMetadata&                        type_map;
			LoaderLogger                               log{};
			base::Optional<code::Function>             function{};
			base::HashMap<base::StrID, usize>          label_positions{};
		};

		i64 getOpCodeArgValue(
			CompilationContext&      ctx,
			const usize              instruction_index,
			const opargs::OpCodeArg& opcode_arg
		) {
			variant_match(opcode_arg) {
				variant_case(vm::opargs::Immediate, imm) return imm.value;

#define HANDLE_OFFSET(Type) variant_case(vm::opargs::Type, offset_type) return offset_type.offset;
				FOR_EACH(HANDLE_OFFSET, VM_OPARG_OFFSET_TYPES);
#undef HANDLE_OFFSET

				variant_case(vm::opargs::Type, type_arg) {
					if (auto type_obj = ctx.type_map.atMaybe(type_arg.type_name))
						return static_cast<i64>(static_cast<u64>(type_obj.value()->getID()));
					ctx.log.log<UnknownTypeError>(type_arg, type_arg.type_name);
					return 0;
				}
				variant_case(vm::opargs::FunctionName, func) {
					for (i64 i = 0; i < ctx.func_map.size(); i++)
						if (ctx.func_map.at(base::safeIntConv<u64>(i))->name == func.function_name)
							return i;
					ctx.log.log<UnknownFunctionError>(func, func.function_name);
					return 0;
				}
				variant_case(vm::opargs::Label, label) {
					auto it = ctx.label_positions.find(label.label_name);
					if (it != ctx.label_positions.end()) {
						// We have to calculate the
						// difference instead of absolute jump position,
						// because our instruction counter is a pointer.
						return static_cast<i64>(it->second) - static_cast<i64>(instruction_index)
						     - 1;
					}
					ctx.log.log<UnknownLabelError>(label);
					return 0;
				}
			}
			CORE_UNREACHABLE();
		}

		low::FuncData changeFuncToFuncData(CompilationContext& ctx) {
			low::FuncData func_data;
			func_data.name = ctx.function->name;
			auto functional_type
				= ctx.type_map.atMaybe(ctx.function->name)
			          .expect<code::builders::MissingFunctionalTypeError>(ctx.function->name);
			func_data.arg_size
				= functional_type->getParametersSize()
			          .expect<code::builders::TypeIsNotFunctionalError>(ctx.function->name);
			// Not expecting here because it's checked above
			func_data.ret_size         = functional_type->getResultType().value()->getSize();
			func_data.local_stack_size = ctx.function->local_stack_size;

			for (usize op_idx = 0; op_idx < ctx.function->body.size(); op_idx++) {
				const auto& op    = ctx.function->body[op_idx];
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
#include <vm/bytecode/opcode_definitions.hpp>
				}

				func_data.bc.emplace_back(Fix8Instruction{
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

		void splitCodeAndLabels(CompilationContext& ctx) {
			code::Function new_func = ctx.function.value();
			new_func.body.clear();
			base::HashMap<base::StrID, usize>                        label_positions;
			base::HashMap<base::StrID, code::instructions::Op_label> labels;
			for (const auto& instr: ctx.function.value().body) {
				variant_match(instr) {
					variant_case(code::instructions::Comment, _);
					variant_case(code::instructions::Op_label, label) {
						auto [_, inserted] = label_positions.insert_or_assign(
							label.arg0.label_name, new_func.body.size()
						);
						if (!inserted) {
							ctx.log.logMap<RepeatedLabelError>(label, [&](auto& err) {
								for (auto&& lbl: label_positions)
									if (lbl.first == label.arg0.label_name) {
										ctx.log.addNote<RepeatedLabelNote>(
											err, labels[label.arg0.label_name]
										);
									}
							});
						} else {
							labels.put(label.arg0.label_name, label);
						}
					}
					variant_default { new_func.body.push_back(instr); }
				}
			}
			ctx.function        = std::move(new_func);
			ctx.label_positions = std::move(label_positions);
		}

	}

	std::expected<low::LowVMProgram, LoaderLogger> compile(const Program& program) {
		std::vector<low::FuncData> converted_functions;
		converted_functions.reserve(program.funcMap().size());

		Box<TypeMetadata> types = program.produceTypeMetadata();
		auto              ctx   = CompilationContext(program.funcMap(), *types);

		for (auto& func: program.funcMap()) {
			ctx.function = func;
			splitCodeAndLabels(ctx);

			auto converted_func = changeFuncToFuncData(ctx);
			converted_functions.push_back(converted_func);
		}

		if (ctx.log.bad()) return std::unexpected(std::move(ctx.log));
		return low::LowVMProgram{ converted_functions, std::move(types) };
	}
}
