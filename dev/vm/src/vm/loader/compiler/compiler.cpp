#include "compiler.hpp"

#include <diagnostic/logger.hpp>

#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
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
			const StableTypeIdNameMap<code::Function>&         func_map;
			const TypeMetadata&                                type_map;
			const StableTypeIdNameMap<TypeCRef, GlobalDataID>& globals;
			LoaderLogger                                       log{};
			base::Optional<code::Function>                     function{};
			base::HashMap<base::StrID, usize>                  label_positions{};
			base::HashMap<base::StrID, usize>                  local_offset_map{};
			usize                                              local_stack_size{};
		};

		i64 getOpCodeArgValue(
			CompilationContext&      ctx,
			const usize              instruction_index,
			const opargs::OpCodeArg& opcode_arg
		) {
			// @TODO After typechecks (#732) most of these checks should probably
			// get removed.
			variant_match(opcode_arg) {
				variant_case(vm::opargs::Immediate, imm) return imm.value;

				// Every used local variable is guaranteed to exist by static verification.
#define HANDLE_LOCAL(TYPE)                                                     \
	variant_case(vm::opargs::TYPE, local_type) {                               \
		return static_cast<i64>(ctx.local_offset_map.at(local_type.var_name)); \
	}
				FOR_EACH(HANDLE_LOCAL, VM_OPARG_LOCAL_TYPES);
#undef HANDLE_LOCAL

#define HANDLE_GLOBAL(TYPE)                                                         \
	variant_case(vm::opargs::TYPE, global_data) {                                   \
		auto name = global_data.global_data_name;                                   \
		if (ctx.globals.contains(name)) return i64(usize(*ctx.globals.idOf(name))); \
		ctx.log.log<UnknownGlobalDataError>(global_data, name);                     \
		return 0;                                                                   \
	}
				FOR_EACH(HANDLE_GLOBAL, VM_OPARG_GLOBAL_TYPES);
#undef HANDLE_GLOBAL

				variant_case(vm::opargs::Type, type_arg) {
					if (auto type_obj = ctx.type_map.atMaybe(type_arg.type_name))
						return static_cast<i64>(static_cast<u64>(type_obj.value()->getID()));
					ctx.log.log<UnknownTypeError>(type_arg, type_arg.type_name);
					return 0;
				}
				variant_case(vm::opargs::FunctionName, func) {
					for (i64 i = 0; i < ctx.func_map.size(); i++)
						if (ctx.func_map.at(base::safeIntConv<u64>(i))->name.str
						    == func.function_name)
							return i;
					ctx.log.log<UnknownFunctionError>(func, func.function_name);
					return 0;
				}
				variant_case(vm::opargs::BuiltinFunctionName, func) {
					auto func_id = builtins::getBuiltinFunctionID(func.function_name);
					if (func_id)
						return base::safeIntConv<i64>(
							static_cast<std::underlying_type_t<builtins::BuiltinFunctionID>>(*func_id
						    )
						);
					ctx.log.log<UnknownFunctionError>(func, func.function_name);
					return 0;
				}
				variant_case(vm::opargs::Label, label) {
					// Labels are guaranteed to exist by static verification.
					auto pos = ctx.label_positions.at(label.label_name);
					// We have to calculate the
					// difference instead of absolute jump position,
					// because our instruction counter is a pointer.
					return static_cast<i64>(pos) - static_cast<i64>(instruction_index) - 1;
				}
			}

			CORE_UNREACHABLE();
		}

		low::FuncData changeFuncToFuncData(CompilationContext& ctx) {
			low::FuncData func_data;
			func_data.name = ctx.function->name.str;
			// This is guaranteed to exist by builders.
			auto functional_type       = ctx.type_map.at(ctx.function->name.str);
			func_data.arg_size         = functional_type->getParametersSize().value();
			func_data.ret_size         = functional_type->getResultType().value()->getSize();
			func_data.local_stack_size = ctx.local_stack_size;

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
			base::HashMap<base::StrID, usize> label_positions;
			for (const auto& instr: ctx.function.value().body) {
				variant_match(instr) {
					variant_case(code::instructions::Comment, _);
					variant_case(code::instructions::Op_label, label) {
						label_positions.put(label.arg0.label_name, new_func.body.size());
					}
					variant_default { new_func.body.push_back(instr); }
				}
			}
			ctx.function        = std::move(new_func);
			ctx.label_positions = std::move(label_positions);
		}

		void calculateOffsets(CompilationContext& ctx) {
			base::HashMap<base::StrID, usize> offsets;
			std::vector<usize>                type_size_stack;
			usize                             curr_stack_size = 0;
			usize                             max_stack_size  = 0;

			auto push = [&](opargs::StackLocalAny local, opargs::Type type) {

				offsets.put(local.var_name, curr_stack_size);
				auto type_size = ctx.type_map.at(type.type_name)->getSize();
				type_size_stack.push_back(type_size);
				curr_stack_size += type_size;
				max_stack_size = std::max(max_stack_size, curr_stack_size);
			};

			auto pop = [&]() {
				auto type_size = type_size_stack.back();
				type_size_stack.pop_back();
				curr_stack_size -= type_size;
			};

			auto func_type = ctx.type_map.at(ctx.function->name)->get<kind::Function>().value();
			push(base::StrID("ret_val"), func_type.result->getName());
			for (auto [idx, param_type]: std::views::enumerate(func_type.parameters))
				push(base::StrID(base::strConcat("arg", idx).c_str()), param_type->getName());

			// instruction index, stack state, stack size
			std::vector<std::tuple<usize, decltype(type_size_stack), usize>> dfs_stack{
				{ ctx.function->body.size(), {}, 0 }  // sentinel
			};
			std::vector<bool> visited_instructions(ctx.function->body.size());
			usize             index = 0;

			while (index != ctx.function->body.size()) {
				if (visited_instructions[index]) {
					std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
					dfs_stack.pop_back();
					continue;
				}
				visited_instructions[index] = true;

				variant_match(ctx.function->body[index]) {
					using namespace code::instructions;
					variant_case(Op_init_lany_type, instr) {
						push(instr.arg0, instr.arg1);
						index++;
					}
					variant_case(Op_deinit, instr) {
						pop();
						index++;
					}
					variant_case(Op_jmp_label, instr) {
						index = ctx.label_positions[instr.arg0.label_name];
					}
					variant_case(Op_jmpIf_label, instr) {
						index++;
						dfs_stack.emplace_back(
							ctx.label_positions[instr.arg0.label_name],
							type_size_stack,
							curr_stack_size
						);
					}
					variant_case(Op_jmpIfNot_label, instr) {
						index++;
						dfs_stack.emplace_back(
							ctx.label_positions[instr.arg0.label_name],
							type_size_stack,
							curr_stack_size
						);
					}
					variant_case(Op_ret, instr) {
						std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
						dfs_stack.pop_back();
					}
					variant_case(Op_call_func, instr) {
						for (usize i = 0;
						     i < ctx.type_map.at(instr.arg0.function_name)->getParameterCount();
						     i++) {
							pop();
						}
						index++;
					}
					variant_case(Op_call_builtin_func, instr) {
						for (usize i = 0;
						     i < ctx.type_map.at(instr.arg0.function_name)->getParameterCount();
						     i++) {
							pop();
						}
						index++;
					}
					variant_case(Op_ret_tailcall_func, instr) {
						std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
						dfs_stack.pop_back();
					}
					variant_default { index++; }
				}
			}

			ctx.local_offset_map = std::move(offsets);
			ctx.local_stack_size = max_stack_size;
		}
	}

	std::expected<low::LowVMProgram, LoaderLogger> compile(const Program& program) {
		std::vector<low::FuncData> converted_functions;
		converted_functions.reserve(program.funcMap().size());

		Box<TypeMetadata> types = program.produceTypeMetadata();

		StableTypeIdNameMap<TypeCRef, GlobalDataID> globals;
		auto ctx = CompilationContext(program.funcMap(), *types, globals);

		for (const auto& global: program.globalMap()) {
			match_optional(types->atMaybe(global.type)) {
				opt_some(type) { globals.insert(type, global.name); }
				opt_none { ctx.log.log<UnknownTypeError>(global.type, global.type.str); }
			}
		}


		for (auto& func: program.funcMap()) {
			ctx.function = func;
			splitCodeAndLabels(ctx);
			calculateOffsets(ctx);

			auto converted_func = changeFuncToFuncData(ctx);
			converted_functions.push_back(converted_func);
		}

		if (ctx.log.bad()) return std::unexpected(std::move(ctx.log));
		return low::LowVMProgram{
			converted_functions,
			std::move(types),
			{ program.globalMap().begin(), program.globalMap().end() },
		};
	}
}
