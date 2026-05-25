#include "compiler.hpp"

#include "instruction_lowering.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/loader/compiler/type_builder.hpp>
#include <vm/utils/interpret.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <ranges>

namespace vm::loader::compiler {
	namespace detail {

#define DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(FAMILY_CONCEPT, ...)            \
	template<FAMILY_CONCEPT ToType>                                           \
	struct LowerArgumentImpl<ToType> {                                        \
		template<opargs::ArgumentType FromType>                               \
		static u64 lower(                                                     \
			[[maybe_unused]] Compiler&                             compiler,  \
			[[maybe_unused]] Compiler::FunctionCompilationContext& ctx,       \
			const FromType&                                        opcode_arg \
		) {                                                                   \
			__VA_ARGS__                                                       \
		}                                                                     \
	}

#define DEFINE_LOWER_ARGUMENT_IMPL(LOW_TO_TYPE, HIGH_FROM_TYPE, ...)          \
	template<>                                                                \
	struct LowerArgumentImpl<LOW_TO_TYPE> {                                   \
		static u64 lower(                                                     \
			[[maybe_unused]] Compiler&                             compiler,  \
			[[maybe_unused]] Compiler::FunctionCompilationContext& ctx,       \
			const HIGH_FROM_TYPE&                                  opcode_arg \
		) {                                                                   \
			__VA_ARGS__                                                       \
		}                                                                     \
	}
		// clang-format off

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::PlaceDataArgumentType,
			if(auto maybe_val = ctx.locals_map.atMaybe(opcode_arg.var_name)) {
				return maybe_val.value()->offset;
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_buffer_offset | (1ULL << 63);
		);

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::PlaceBlockArgumentType,
			if(auto maybe_val = ctx.locals_map.atMaybe(opcode_arg.var_name)) {
				return maybe_val.value()->block_idx;
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_block_idx | (1ULL << 63);
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::Type,
			opargs::Type,
			return safeReadObjectBytes<u64>(compiler.low_program.getTypes().at(opcode_arg.type_name));
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::Field,
			opargs::Field,
			return static_cast<u64>(*compiler.low_program.getTypes()
		                                 .at(opcode_arg.type_name)
		                                 ->getFieldOffsetByName(opcode_arg.field_name));
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::FunctionID,
		    opargs::FunctionName,
		    return compiler.program_ctx.function_forward_declarations.idOf(opcode_arg.function_name).value();
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::BuiltinFunctionID,
			opargs::BuiltinFunctionName,
			auto func_id = *builtins::getBuiltinFunctionID(opcode_arg.function_name);
			return base::safeIntConv<u64>(
				static_cast<std::underlying_type_t<builtins::BuiltinFunctionID>>(func_id)
			);
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::ExtCFunction,
			opargs::ExtCFunctionName,
			return safeReadObjectBytes<u64>(compiler.low_program.getExternCFunctions().at(opcode_arg.function_name));
		);
		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::MethodName,
			opargs::MethodName,
			return base::safeIntConv<u64>(compiler.program_ctx.method_name_to_id[opcode_arg.method_name]);
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::Immediate,
			opargs::Immediate,
			return opcode_arg.value;
		);

		DEFINE_LOWER_ARGUMENT_IMPL(
			low::opargs::Label,
			opargs::Label,
			// Lower the label names into temporary label IDs.
			// A label ID is some number, used later by `linkLabelArguments`
			// to generate actual offsets once we know where each label
			// lands after lowering.
			if (!ctx.label_id_map.contains(opcode_arg.label_name))
				ctx.label_id_map.put(opcode_arg.label_name, ctx.label_id_map.size());
			return ctx.label_id_map.at(opcode_arg.label_name);
		);
		// clang-format on

#undef DEFINE_LOWER_ARGUMENT_IMPL
#undef DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY
	}  // namespace detail

	template<opargs::ArgumentType FromType, low::opargs::ArgumentType ToType>
	u64 Compiler::lowerArgument(FunctionCompilationContext& ctx, const FromType& opcode_arg) {
		return detail::LowerArgumentImpl<ToType>::lower(*this, ctx, opcode_arg);
	}

	void Compiler::linkLabelArguments(
		low::MicroBytecode& instructions, const base::HashMap<usize, usize>& label_map
	) {
		for (auto [instr_idx, instr]: std::views::enumerate(instructions)) {
			auto       opcode_num = std::to_underlying(getInstructionOpcode(instr));
			std::array args{ Ref(&instr.arg0), Ref(&instr.arg1) };
			auto       are_args_labels = low::instruction_tags::IS_ARGUMENT_LABEL.at(opcode_num);

			for (auto [arg, is_label]: std::views::zip(args, are_args_labels))
				if (is_label) *arg = label_map.at(*arg) - static_cast<usize>(instr_idx) - 1;
		}
	}

	low::MicroBytecode Compiler::lowerInstructions(FunctionCompilationContext& ctx) {
		detail::MicroBytecodeBuilder builder{ *this, ctx };

		for (auto [idx, instr]: std::views::enumerate(ctx.function.body)) {
			ctx.current_instruction_index = idx;
			auto instruction_range        = builder.add(instr);
			ctx.instruction_mapping.push_back(instruction_range);
		}

		auto [micro_bytecode, label_map] = builder.build();
		linkLabelArguments(micro_bytecode, label_map);

		return micro_bytecode;
	}

	base::Optional<std::pair<u64, u64>> Compiler::seekMethodParamRetCount(
		const base::StrID& method_name
	) const {
		// @TODO: #962 Optimize
		auto it = std::ranges::find_if(*low_program.types, [&](const auto& type) {
			if_opt_some(type.getInheritanceMetadata(), inh_meta) {
				return (*inh_meta).available_methods.contains(method_name);
			}
			return false;
		});
		if (it != low_program.types->end()) {
			auto inh_meta = it->getInheritanceMetadata().value();
			auto param    = inh_meta->available_methods[method_name]->getParameterCount();
			auto ret      = inh_meta->available_methods[method_name]->getResultTypeCount();
			if (!param)
				return std::nullopt;  // Either both param and ret are present or both are not.
			return std::make_pair(*param, *ret);
		}
		CORE_UNREACHABLE();
	}

	base::HashMap<base::StrID, usize> Compiler::calculateLabelsMapping(FunctionCompilationContext& ctx
	) const {
		base::HashMap<base::StrID, usize> label_positions{};
		for (auto [idx, instr]: std::views::enumerate(ctx.function.body)) {
			instr_match(instr) {
				instr_case(code::instructions::Op_label, label) {
					label_positions.put(label.label.label_name, idx);
				}
				instr_default {}
			}
		}
		return label_positions;
	}

	std::unordered_set<base::StrID> Compiler::detectParamsAndReturnedVars(
		FunctionCompilationContext& ctx
	) const {
		std::vector<base::StrID>        stack;
		std::unordered_set<base::StrID> res;

		auto push = [&](opargs::PlaceAny local) { stack.push_back(local.var_name); };

		auto pop = [&]() { stack.pop_back(); };

		auto register_top_of_stack_as_arg_or_ret = [&](size_t cnt = 1) {
			for (size_t i = 0; i < cnt; ++i) res.insert(stack[stack.size() - 1 - i]);
		};

		// Pops all arguments, registers all arguments and return values.
		auto handle_non_tailcall = [&](size_t args_cnt, size_t ret_cnt) {
			register_top_of_stack_as_arg_or_ret(args_cnt + ret_cnt);
			for (size_t i = 0; i < args_cnt; ++i) pop();
		};

		base::HashMap<base::StrID, usize> label_positions = calculateLabelsMapping(ctx);

		const code::FuncSignature& func_signature = ctx.function.signature;

		// Add return values and params to stack, all of them must be fully alligned.
		for (auto [idx, ret_type]: std::views::enumerate(func_signature.result_types))
			push(base::StrID(base::strConcat("ret", idx).c_str()));
		for (auto [idx, param_type]: std::views::enumerate(func_signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()));
		register_top_of_stack_as_arg_or_ret(
			func_signature.result_types.size() + func_signature.parameters.size()
		);

		// At each label stack has same state.
		base::HashMap<usize, decltype(stack)> stack_at_label;

		std::vector<std::tuple<usize, decltype(stack)>> dfs_stack{
			{ ctx.function.body.size(), {} }  // sentinel
		};
		usize index = 0;

		while (index != ctx.function.body.size()) {
			instr_match(ctx.function.body[index]) {
				using namespace code::instructions;
				instr_case(Op_label, instr) {
					match_optional(stack_at_label.atMaybe(index)) {
						opt_some(_) {  // State was already visited.
							std::tie(index, stack) = dfs_stack.back();
							dfs_stack.pop_back();
						}
						opt_none {
							stack_at_label.put(index, stack);
							index++;
						}
					}
				}
				instr_case(Op_init_pany_type, instr) {
					push(instr.var);
					index++;
				}
				instr_case(Op_deinit, instr) {
					pop();
					index++;
				}
				instr_case(Op_jmp_label, instr) { index = label_positions[instr.label.label_name]; }
				instr_case(Op_jmpIf_label, instr) {
					index++;
					dfs_stack.emplace_back(label_positions[instr.label.label_name], stack);
				}
				instr_case(Op_jmpIfNot_label, instr) {
					index++;
					dfs_stack.emplace_back(label_positions[instr.label.label_name], stack);
				}
				instr_case(Op_ret, instr) {
					std::tie(index, stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_case(Op_call_func, instr) {
					const auto signature = program_ctx.function_forward_declarations
					                           .at(instr.function.function_name)
					                           ->signature;
					auto param_count = signature.parameters.size();
					auto ret_count   = signature.result_types.size();
					handle_non_tailcall(param_count, ret_count);
					index++;
				}
				instr_case(Op_call_builtinfunc, instr) {
					const auto signature
						= builtins::getBuiltinFunctionSignature(instr.function.function_name).value();
					auto param_count = signature->parameters.size();
					auto ret_count   = signature->result_types.size();
					handle_non_tailcall(param_count, ret_count);
					index++;
				}
				instr_case(Op_call_cfunc, instr) {
					const auto signature
						= program_ctx.ext_c_functions.at(instr.function.function_name)->signature;
					auto param_count = signature.parameters.size();
					auto ret_count   = signature.result_types.size();
					handle_non_tailcall(param_count, ret_count);
					index++;
				}
				instr_case(Op_virtual_call_pptr_method, instr) {
					auto [param_count, ret_count]
						= *seekMethodParamRetCount(instr.method.method_name);
					handle_non_tailcall(param_count, ret_count);
					index++;
				}
				instr_case(Op_ret_tailcall_func, instr) {
					// Stack contains exactly parameters and ret values of that call.
					// Everything is passed and must be fully aligned.
					register_top_of_stack_as_arg_or_ret(stack.size());

					std::tie(index, stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_default { index++; }
			}
		}
		return res;
	}

	void Compiler::calculateOffsets(FunctionCompilationContext& ctx) {
		const auto must_be_fully_alligned = detectParamsAndReturnedVars(ctx);

		decltype(ctx.locals_map) result;
		std::vector<usize>       type_size_stack;
		std::vector<usize>       padding_used;
		usize                    curr_stack_size = 0;
		usize                    max_stack_size  = 0;
		usize                    max_block_count = 0;
		ctx.offset_at_instructions               = std::vector<usize>(ctx.function.body.size(), -1);

		usize index = 0;

		// instruction index, type stack state, padding stack state, stack size
		std::vector<std::tuple<usize, decltype(type_size_stack), decltype(padding_used), usize>>
			dfs_stack{
				{ ctx.function.body.size(), {}, {}, 0 }  // sentinel
			};
		std::vector<bool> visited_instructions(ctx.function.body.size());

		auto push = [&](opargs::PlaceAny local, opargs::Type type) {
			auto type_ref   = low_program.types->at(type.type_name);
			auto type_size  = type_ref->getSize().asInt();
			auto type_align = must_be_fully_alligned.contains(local.var_name)
			                    ? Type::MAX_ALIGNMENT
			                    : type_ref->getStackAlignment();

			size_t padding        = Type::alignUp(curr_stack_size, type_align) - curr_stack_size;
			auto   aligned_offset = curr_stack_size + padding;

			if_opt_some(result.atMaybe(local.var_name), entry) {
				if (entry->offset != aligned_offset) {
					CORE_PANIC(
						"DuplicatedLocalNameError - used a variable again at a different offset "
						"which wasn't detected by the function validator"
					);
				}
			}
			result.put(
				local.var_name,
				{ .offset = aligned_offset, .block_idx = type_size_stack.size(), .type = type_ref }
			);
			ctx.offset_at_instructions[index] = aligned_offset;
			padding_used.push_back(padding);
			type_size_stack.push_back(type_size);
			curr_stack_size += padding + type_size;
			max_stack_size  = std::max(max_stack_size, curr_stack_size);
			max_block_count = std::max(max_block_count, type_size_stack.size());
		};

		auto pop = [&]() {
			auto type_size = type_size_stack.back();
			auto padding   = padding_used.back();
			type_size_stack.pop_back();
			padding_used.pop_back();
			curr_stack_size -= type_size;
			curr_stack_size -= padding;
		};

		auto handle_call = [&](usize params_cnt) {
			ctx.offset_at_instructions[index] = curr_stack_size;
			for (usize i = 0; i < params_cnt; i++) pop();
		};

		base::HashMap<base::StrID, usize> label_positions = calculateLabelsMapping(ctx);

		code::FuncSignature func_signature = ctx.function.signature;
		using namespace std::views;
		for (auto [idx, ret_type]: enumerate(func_signature.result_types))
			push(base::StrID(base::strConcat("ret", idx).c_str()), ret_type.str);
		ctx.return_stack_size = curr_stack_size;
		for (auto [idx, param_type]: enumerate(func_signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param_type.str);
		ctx.shared_stack_size = curr_stack_size;

		while (index != ctx.function.body.size()) {
			if (visited_instructions[index]) {
				std::tie(index, type_size_stack, padding_used, curr_stack_size) = dfs_stack.back();
				dfs_stack.pop_back();
				continue;
			}
			visited_instructions[index] = true;

			instr_match(ctx.function.body[index]) {
				using namespace code::instructions;
				instr_case(Op_init_pany_type, instr) {
					push(instr.var, instr.type);
					index++;
				}
				instr_case(Op_deinit, instr) {
					pop();
					index++;
				}
				instr_case(Op_jmp_label, instr) { index = label_positions[instr.label.label_name]; }
				instr_case(Op_jmpIf_label, instr) {
					index++;
					dfs_stack.emplace_back(
						label_positions[instr.label.label_name],
						type_size_stack,
						padding_used,
						curr_stack_size
					);
				}
				instr_case(Op_jmpIfNot_label, instr) {
					index++;
					dfs_stack.emplace_back(
						label_positions[instr.label.label_name],
						type_size_stack,
						padding_used,
						curr_stack_size
					);
				}
				instr_case(Op_ret, instr) {
					std::tie(index, type_size_stack, padding_used, curr_stack_size)
						= dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_case(Op_call_func, instr) {
					handle_call(program_ctx.function_forward_declarations
					                .at(instr.function.function_name)
					                ->signature.parameters.size());
					index++;
				}
				instr_case(Op_call_builtinfunc, instr) {
					handle_call(builtins::getBuiltinFunctionSignature(instr.function.function_name)
					                .value()
					                ->parameters.size());
					index++;
				}
				instr_case(Op_call_cfunc, instr) {
					handle_call(program_ctx.ext_c_functions.at(instr.function.function_name)
					                ->signature.parameters.size());
					index++;
				}
				instr_case(Op_virtual_call_pptr_method, instr) {
					handle_call(seekMethodParamRetCount(instr.method.method_name)->first);
					index++;
				}
				instr_case(Op_ret_tailcall_func, instr) {
					std::tie(index, type_size_stack, padding_used, curr_stack_size)
						= dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_default { index++; }
			}
		}

		ctx.locals_map        = std::move(result);
		ctx.local_stack_size  = max_stack_size;
		ctx.local_block_count = max_block_count;
	}

	void Compiler::compileNewFunctions(const std::vector<code::Function>& new_functions) {
		// Forward declare all functions
		for (const auto& function: new_functions)
			program_ctx.function_forward_declarations.insert(function, function.name);

		for (const auto& function: new_functions) {
			FunctionCompilationContext ctx(function);
			calculateOffsets(ctx);

			// Calculate the functions metadata.
			code::FuncSignature   signature = function.signature;
			std::vector<TypeCRef> parameters;
			parameters.reserve(signature.parameters.size());

			for (const auto& param: signature.parameters) {
				auto type = low_program.types->at(param.str);
				parameters.emplace_back(type);
			}

			low::MicroBytecode bytecode = lowerInstructions(ctx);

			std::vector<TypeCRef> result_types = {};
			for (auto& ret: signature.result_types)
				result_types.emplace_back(low_program.types->at(ret));

			low_program.functions.insert(
				low::LowFuncData{ .name              = function.name,
			                      .bc                = std::move(bytecode),
			                      .local_stack_size  = ctx.local_stack_size,
			                      .local_block_count = ctx.local_block_count,
			                      .arg_size     = ctx.shared_stack_size - ctx.return_stack_size,
			                      .ret_size     = ctx.return_stack_size,
			                      .parameters   = std::move(parameters),
			                      .result_types = std::move(result_types),
			                      .instruction_mapping = std::move(ctx.instruction_mapping) },
				function.name
			);
		}
	}

	void Compiler::compileNewGlobals(const std::vector<code::GlobalData>& new_globals) {
		for (const auto& global: new_globals) {
			base::Optional<base::StrID> ctor_name, dtor_name;
			if (global.ctor_name.has_value()) ctor_name = global.ctor_name->str;
			if (global.dtor_name.has_value()) dtor_name = global.dtor_name->str;
			auto type                      = low_program.types->at(global.type);
			program_ctx.global_buffer_size = Bytes(
				Type::alignUp(program_ctx.global_buffer_size.asInt(), type->getStackAlignment())
			);

			low::LowGlobalData data{
				.type                 = type,
				.ctor_name            = ctor_name,
				.dtor_name            = dtor_name,
				.global_buffer_offset = program_ctx.global_buffer_size.asInt(),
				.global_block_idx     = program_ctx.global_count,
			};
			low_program.global_data.insert(data, global.name);

			program_ctx.global_count += 1;
			program_ctx.global_buffer_size += Bytes(data.type->getSize().asInt());
		}

		low_program.global_buffer_size = program_ctx.global_buffer_size;
		low_program.global_count       = program_ctx.global_count;
	}

	void Compiler::compileNewTypes(const code::TypeContext& ctx) {
		auto new_types = ctx.getCurrentTypes() | std::views::drop(low_program.types->size());
		if (std::ranges::empty(new_types)) return;

		vm::code::detail::rebuildTypeMetadata(low_program.types.refMut(), ctx.getCurrentTypes());

		// Update method ID to name maps, since new methods may have appeared after new types were
		// added.
		for (const auto& new_type: new_types) {
			auto type_from_metadata = low_program.types->at(new_type.getName());
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

	void Compiler::compileNewExtCFunctions(const std::vector<code::ExternalCFunction>& new_functions
	) {
		for (const auto& new_func: new_functions) {
			program_ctx.ext_c_functions.insert(new_func, new_func.name);
			std::vector<TypeCRef> params = new_func.signature.parameters
			                             | std::views::transform([this](const auto& param_name) {
											   return low_program.types->at(param_name);
										   })
			                             | std::ranges::to<std::vector<TypeCRef>>();

			std::vector<TypeCRef> rets = new_func.signature.result_types
			                           | std::views::transform([this](const auto& param_name) {
											 return low_program.types->at(param_name);
										 })
			                           | std::ranges::to<std::vector<TypeCRef>>();

			low_program.extern_c_functions.insert(
				low::LowExternCFunction{
					.name             = new_func.name,
					.function_pointer = new_func.function_pointer,
					.parameters       = std::move(params),
					.result_types     = std::move(rets),
				},
				new_func.name
			);
		}
	}

	void Compiler::recompile(const code::ValidProgram& high_program) {
		compileNewTypes(high_program.getTypeContext());

		auto new_c_functions = high_program.extCFunctions()
		                     | std::views::drop(low_program.extern_c_functions.size())
		                     | std::ranges::to<std::vector<code::ExternalCFunction>>();
		compileNewExtCFunctions(new_c_functions);

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
