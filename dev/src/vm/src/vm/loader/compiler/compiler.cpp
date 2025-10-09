#include "compiler.hpp"

#include <base/int_conv.hpp>
#include <base/ints.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/type_builder.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/parser/elements.hpp>
#include <vm/loader/parser/errors.hpp>
#include <vm/utils/interpret.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <ranges>

namespace vm::loader::compiler {
	u64 Compiler::lowerArgument(
		const FunctionCompilationContext& ctx,
		const usize                       instruction_index,
		const opargs::OpCodeArg&          opcode_arg
	) {
		variant_match(opcode_arg) {
			variant_case(vm::opargs::Immediate, imm) return imm.value;

			// Every used local variable is guaranteed to exist by static verification.
#define HANDLE_LOCAL(TYPE)                                                     \
	variant_case(vm::opargs::TYPE, local_type) {                               \
		return static_cast<u64>(ctx.local_offset_map.at(local_type.var_name)); \
	}
			FOR_EACH(HANDLE_LOCAL, VM_OPARG_LOCAL_TYPES);
#undef HANDLE_LOCAL

#define HANDLE_GLOBAL(TYPE)                                     \
	variant_case(vm::opargs::TYPE, global_data) {               \
		auto name = global_data.global_data_name;               \
		return u64(usize(*low_program.global_data.idOf(name))); \
	}
			FOR_EACH(HANDLE_GLOBAL, VM_OPARG_GLOBAL_TYPES);
#undef HANDLE_GLOBAL

			variant_case(vm::opargs::Type, type_arg) {
				auto type_obj = low_program.types->at(type_arg.type_name);
				return static_cast<u64>(static_cast<u64>(type_obj->getID()));
			}
			variant_case(vm::opargs::Field, field_arg) {
				auto type_obj     = low_program.types->at(field_arg.type_name);
				auto field_offset = *type_obj->getFieldOffsetByName(field_arg.field_name);
				return static_cast<u64>(field_offset);
			}
			variant_case(vm::opargs::FunctionName, func) {
				return u64(*program_ctx.function_forward_declarations.idOf(func.function_name));
			}
			variant_case(vm::opargs::BuiltinFunctionName, func) {
				auto func_id = *builtins::getBuiltinFunctionID(func.function_name);
				return base::safeIntConv<u64>(
					static_cast<std::underlying_type_t<builtins::BuiltinFunctionID>>(func_id)
				);
			}
			variant_case(vm::opargs::MethodName, method) {
				return base::safeIntConv<u64>(program_ctx.method_name_to_id[method.method_name]);
			}
			variant_case(vm::opargs::Label, label) {
				// Labels are guaranteed to exist by static verification.
				auto pos = ctx.label_positions.at(label.label_name);
				// We have to calculate the
				// difference instead of absolute jump position,
				// because our instruction index is a pointer.
				return static_cast<u64>(pos) - static_cast<u64>(instruction_index) - 1;
			}
			variant_default { CORE_PANIC("Unhandled OpCode argument type"); }
		}
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Compiles a single function. Refactor this
	 */
	low::MicroBytecode Compiler::lowerInstructions(const FunctionCompilationContext& ctx) {
		low::MicroBytecode bc;
		bc.reserve(ctx.instructions_without_labels.size());

		for (usize op_idx = 0; op_idx < ctx.instructions_without_labels.size(); ++op_idx) {
			const code::Instruction& op    = ctx.instructions_without_labels[op_idx];
			u64                      arg_0 = 0;
			u64                      arg_1 = 0;

			variant_match(op) {
#define HANDLE_OPCODE_0ARGS(opcode) \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {}
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)          \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {   \
		arg_0 = lowerArgument(ctx, op_idx, instr.arg0); \
	}

#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) \
	variant_case(VM_INSTR_FROM_NAME(opcode), instr) {     \
		arg_0 = lowerArgument(ctx, op_idx, instr.arg0);   \
		arg_1 = lowerArgument(ctx, op_idx, instr.arg1);   \
	}
#include <vm/bytecode/instruction_definitions.hpp>
			}

			auto opcode_id = low::fix8FromInstr(op);
			bc.emplace_back(makeLowInstruction(opcode_id, arg_0, arg_1));
#if defined(BUILD_TYPE_DEV_DEBUG)
			bc.back().representation = instructionToString(op);
			bc.back().opcode_id      = opcode_id;
#endif
		}
		return bc;
	}

	void Compiler::calculateOffsets(FunctionCompilationContext& ctx) {
		base::HashMap<base::StrID, usize> offsets;
		std::vector<usize>                type_size_stack;
		usize                             curr_stack_size = 0;
		usize                             max_stack_size  = 0;

		auto push = [&](opargs::StackLocalAny local, opargs::Type type) {
			if_opt_some(offsets.atMaybe(local.var_name), offset) {
				if (*offset != curr_stack_size) {
					CORE_PANIC(
						"DuplicatedLocalNameError - used a variable again at a different offset "
						"which wasn't detected by the function validator"
					);
				}
			}

			offsets.put(local.var_name, curr_stack_size);
			auto type_size = low_program.types->at(type.type_name)->getSize();
			type_size_stack.push_back(type_size);
			if (type.type_name == "void") return;
			curr_stack_size += type_size;
			max_stack_size = std::max(max_stack_size, curr_stack_size);
		};

		auto pop = [&]() {
			auto type_size = type_size_stack.back();
			type_size_stack.pop_back();
			curr_stack_size -= type_size;
		};

		auto seek_method_param_count = [&](const base::StrID& method_name) -> base::Optional<u64> {
			// @todo: https://github.com/ducktype-org/duckling/issues/962
			auto it = std::ranges::find_if(*low_program.types, [&](const auto& type) {
				if_opt_some(type.getInheritanceMetadata(), inh_meta) {
					return (*inh_meta).virtual_methods.contains(method_name);
				}
				return false;
			});
			if (it != low_program.types->end()) {
				auto inh_meta = it->getInheritanceMetadata().value();
				return inh_meta->virtual_methods[method_name]->getParameterCount();
			}
			CORE_UNREACHABLE();
		};

		code::FuncSignature func_signature = ctx.function.signature;
		push(base::StrID("ret_val"), func_signature.result_type.str);
		for (auto [idx, param_type]: std::views::enumerate(func_signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param_type.str);
		// instruction index, stack state, stack size
		std::vector<std::tuple<usize, decltype(type_size_stack), usize>> dfs_stack{
			{ ctx.instructions_without_labels.size(), {}, 0 }  // sentinel
		};
		std::vector<bool> visited_instructions(ctx.instructions_without_labels.size());
		usize             index = 0;

		while (index != ctx.instructions_without_labels.size()) {
			if (visited_instructions[index]) {
				std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
				dfs_stack.pop_back();
				continue;
			}
			visited_instructions[index] = true;

			variant_match(ctx.instructions_without_labels[index]) {
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
						ctx.label_positions[instr.arg0.label_name], type_size_stack, curr_stack_size
					);
				}
				variant_case(Op_jmpIfNot_label, instr) {
					index++;
					dfs_stack.emplace_back(
						ctx.label_positions[instr.arg0.label_name], type_size_stack, curr_stack_size
					);
				}
				variant_case(Op_ret, instr) {
					std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				variant_case(Op_call_func, instr) {
					usize number_of_params
						= program_ctx.function_forward_declarations.at(instr.arg0.function_name)
					          ->signature.parameters.size();
					for (usize i = 0; i < number_of_params; i++) pop();
					index++;
				}
				variant_case(Op_call_builtin_func, instr) {
					for (usize i = 0;
					     i < builtins::getBuiltinFunctionSignature(instr.arg0.function_name)
					             .value()
					             ->parameters.size();
					     i++) {
						pop();
					}
					index++;
				}
				variant_case(Op_virtual_call_lptr_method, instr) {
					for (usize i = 0; i < *seek_method_param_count(instr.arg1.method_name); i++)
						pop();
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

	void Compiler::splitCodeAndLabels(FunctionCompilationContext& ctx) {
		std::vector<code::Instruction>    instructions_without_labels;
		base::HashMap<base::StrID, usize> label_positions;
		for (const auto& instr: ctx.function.body) {
			variant_match(instr) {
				variant_case_novalue(code::instructions::Comment) {}
				variant_case(code::instructions::Op_label, label) {
					label_positions.put(label.arg0.label_name, instructions_without_labels.size());
				}
				variant_default { instructions_without_labels.push_back(instr); }
			}
		}
		ctx.instructions_without_labels = std::move(instructions_without_labels);
		ctx.label_positions             = std::move(label_positions);
	}

	void Compiler::compileNewFunctions(const std::vector<code::Function>& new_functions) {
		// Forward declare all functions
		for (const auto& function: new_functions)
			program_ctx.function_forward_declarations.insert(function, function.name);

		for (const auto& function: new_functions) {
			FunctionCompilationContext ctx(function);
			splitCodeAndLabels(ctx);
			calculateOffsets(ctx);

			// Calculate the functions metadata.
			code::FuncSignature   signature       = function.signature;
			usize                 parameters_size = 0;
			std::vector<TypeCRef> parameters;
			parameters.reserve(signature.parameters.size());

			for (const auto& param: signature.parameters) {
				auto type = low_program.types->at(param.str);
				parameters.emplace_back(type);
				parameters_size += type->getSize();
			}

			low::MicroBytecode bytecode = lowerInstructions(ctx);

			low_program.functions.insert(
				low::LowFuncData{ .name             = function.name,
			                      .bc               = std::move(bytecode),
			                      .local_stack_size = ctx.local_stack_size,
			                      .arg_size         = parameters_size,
			                      .ret_size
			                      = low_program.types->at(signature.result_type)->getSize(),
			                      .parameters  = std::move(parameters),
			                      .result_type = low_program.types->at(signature.result_type) },
				function.name

			);
		}
	}

	void Compiler::compileNewGlobals(const std::vector<code::GlobalData>& new_globals) {
		for (const auto& global: new_globals) {
			base::Optional<base::StrID> ctor_name, dtor_name;
			if (global.ctor_name.has_value()) ctor_name = global.ctor_name->str;
			if (global.dtor_name.has_value()) dtor_name = global.dtor_name->str;

			low::LowGlobalData data{
				.type      = low_program.types->at(global.type),
				.ctor_name = ctor_name,
				.dtor_name = dtor_name,
			};
			low_program.global_data.insert(data, global.name);
		}
	}

	void Compiler::compileNewTypes(const code::TypeContext& ctx) {
		auto new_types = ctx.getCurrentTypes() | std::views::drop(low_program.types->size());
		if (std::ranges::empty(new_types)) return;

		vm::code::detail::rebuildTypeMetadata(low_program.types.refMut(), ctx);

		// Update method ID to name maps, since new methods may have appeared after new types where
		// added.
		for (const auto& new_type: new_types) {
			auto type_from_metadata = low_program.types->at(typeName(new_type));
			if_opt_some(type_from_metadata->getInheritanceMetadata(), metadata) {
				//@todo: https://github.com/ducktype-org/duckling/issues/962
				for (auto& [name, impl]: metadata->vtable) {
					if (!program_ctx.method_name_to_id.contains(name)) {
						u64 new_id = program_ctx.method_name_to_id.size();
						program_ctx.method_name_to_id.put(name, new_id);
						low_program.method_name_pool.put(new_id, name);
					}
				}
			}
		}
	}

	void Compiler::recompile(const code::ValidProgram& high_program) {
		compileNewTypes(high_program.getTypeContext());

		auto new_globals = high_program.globals() | std::views::drop(low_program.global_data.size())
		                 | std::ranges::to<std::vector<code::GlobalData>>();

		compileNewGlobals(new_globals);

		auto new_functions = high_program.functions()
		                   | std::views::drop(low_program.functions.size())
		                   | std::ranges::to<std::vector<code::Function>>();
		compileNewFunctions(new_functions);
	}

	CRef<low::LowVMProgram> Compiler::getLowProgram() const { return &low_program; }
}
