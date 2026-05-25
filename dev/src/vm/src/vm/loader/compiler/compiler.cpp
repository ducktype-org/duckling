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
			low::opargs::ShadowPlaceDataArgumentType,
			if(auto maybe_val = ctx.locals_map.atMaybe(opcode_arg.var_name)) {
				return maybe_val.value()->shadow_offset;
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_shadow_data_offset | (1ULL << 63);
		);

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::ShadowPointerPlaceDataArgumentType,
			if(auto maybe_val = ctx.locals_map.atMaybe(opcode_arg.var_name)) {
				return maybe_val.value()->shadow_pointer_offset;
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_shadow_pointer_offset | (1ULL << 63);
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
            low::opargs::ShadowField,
            opargs::Field,
            return static_cast<u64>(*compiler.low_program.getTypes()
                                    .at(opcode_arg.type_name)
                                    ->getFieldShadowOffsetByName(opcode_arg.field_name));
        );

        DEFINE_LOWER_ARGUMENT_IMPL(
            low::opargs::ShadowPointerField,
            opargs::Field,
            return static_cast<u64>(*compiler.low_program.getTypes()
                                    .at(opcode_arg.type_name)
                                    ->getFieldPointerOffsetByName(opcode_arg.field_name));
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

		for (const auto& instr: ctx.function.body) builder.add(instr);

		auto [micro_bytecode, label_map] = builder.build();
		linkLabelArguments(micro_bytecode, label_map);

		return micro_bytecode;
	}

	namespace {
		void populateOffsets(
			std::vector<u32>& data_offsets,
			std::vector<u32>& ptr_offsets,
			usize base_byte,
			usize base_shadow,
			usize base_ptr,
			TypeCRef type
		) {
			variant_match(type->getKindVariant()) {
				variant_case(kind::FixedSizeTable, fixed_size_table) {
					auto inner = fixed_size_table.inner_type;
					usize inner_sz = inner->getSize().asInt();
					usize inner_sh = inner->getShadowSize();
					usize inner_ptr = inner->getPointerSize();
					for (usize el = 0; el < fixed_size_table.element_count; ++el) {
						populateOffsets(
							data_offsets,
							ptr_offsets,
							base_byte + el * inner_sz,
							base_shadow + el * inner_sh,
							base_ptr + el * inner_ptr,
							inner
						);
					}
				}
				variant_case(kind::Data, data) {
					for (const auto& field : data.fields) {
						populateOffsets(
							data_offsets,
							ptr_offsets,
							base_byte + field.offset.asInt(),
							base_shadow + field.shadow_offset,
							base_ptr + field.pointer_offset,
							field.type
						);
					}
				}
				variant_case(kind::Variant, variant) {
					for (usize i = 0; i < variant.type_tag_size.asInt(); ++i) {
						if (base_byte + i < data_offsets.size()) {
							data_offsets[base_byte + i] = static_cast<u32>(base_shadow);
							ptr_offsets[base_byte + i] = static_cast<u32>(base_ptr);
						}
					}
					for (const auto& alt : variant.alternatives) {
						populateOffsets(
							data_offsets,
							ptr_offsets,
							base_byte + variant.type_tag_size.asInt(),
							base_shadow + 1,
							base_ptr,
							alt
						);
					}
				}
				variant_default {
					usize sz = type->getSize().asInt();
					for (usize i = 0; i < sz; ++i) {
						if (base_byte + i < data_offsets.size()) {
							data_offsets[base_byte + i] = static_cast<u32>(base_shadow);
							ptr_offsets[base_byte + i] = static_cast<u32>(base_ptr);
						}
					}
				}
			}
		}
	}

	void Compiler::calculateOffsets(FunctionCompilationContext& ctx) {
		decltype(ctx.locals_map) result;
		std::vector<usize>       type_size_stack;
		std::vector<usize>       type_shadow_size_stack;
		std::vector<usize>       type_shadow_pointer_size_stack;
		usize                    curr_stack_size = 0;
		usize                    curr_shadow_size = 0;
		usize                    curr_shadow_pointer_size = 0;
		usize                    max_stack_size  = 0;
		usize                    max_block_count = 0;

		auto push = [&](opargs::PlaceAny local, opargs::Type type) {
			if_opt_some(result.atMaybe(local.var_name), entry) {
				if (entry->offset != curr_stack_size) {
					CORE_PANIC(
						"DuplicatedLocalNameError - used a variable again at a different offset "
						"which wasn't detected by the function validator"
					);
				}
			}

			auto type_ref = low_program.types->at(type.type_name);
			result.put(
				local.var_name,
				{
					.offset = curr_stack_size,
					.block_idx = type_size_stack.size(),
					.type = type_ref,
					.shadow_offset = curr_shadow_size,
					.shadow_pointer_offset = curr_shadow_pointer_size
				}
			);
			auto type_size = type_ref->getSize().asInt();
			auto type_shadow = type_ref->getShadowSize();
			auto type_ptr = type_ref->getPointerSize();

			type_size_stack.push_back(type_size);
			type_shadow_size_stack.push_back(type_shadow);
			type_shadow_pointer_size_stack.push_back(type_ptr);

			curr_stack_size += type_size;
			curr_shadow_size += type_shadow;
			curr_shadow_pointer_size += type_ptr;

			max_stack_size  = std::max(max_stack_size, curr_stack_size);
			max_block_count = std::max(max_block_count, type_size_stack.size());
		};

		auto pop = [&]() {
			auto type_size = type_size_stack.back();
			type_size_stack.pop_back();
			curr_stack_size -= type_size;

			auto type_shadow = type_shadow_size_stack.back();
			type_shadow_size_stack.pop_back();
			curr_shadow_size -= type_shadow;

			auto type_ptr = type_shadow_pointer_size_stack.back();
			type_shadow_pointer_size_stack.pop_back();
			curr_shadow_pointer_size -= type_ptr;
		};

		auto seek_method_param_count = [&](const base::StrID& method_name) -> base::Optional<u64> {
			// @todo: https://github.com/ducktype-org/duckling/issues/962
			auto it = std::ranges::find_if(*low_program.types, [&](const auto& type) {
				if_opt_some(type.getInheritanceMetadata(), inh_meta) {
					return (*inh_meta).available_methods.contains(method_name);
				}
				return false;
			});
			if (it != low_program.types->end()) {
				auto inh_meta = it->getInheritanceMetadata().value();
				return inh_meta->available_methods[method_name]->getParameterCount();
			}
			CORE_UNREACHABLE();
		};

		// Label positions in high bytecode, used only for graph traversing
		// in this function. Not used when lowering to microbytecode.
		base::HashMap<base::StrID, usize> label_positions{};
		for (auto [idx, instr]: std::views::enumerate(ctx.function.body)) {
			instr_match(instr) {
				instr_case(code::instructions::Op_label, label) {
					label_positions.put(label.label.label_name, idx);
				}
				instr_default {}
			}
		}

		code::FuncSignature func_signature = ctx.function.signature;
		using namespace std::views;
		for (auto [idx, ret_type]: enumerate(func_signature.result_types))
			push(base::StrID(base::strConcat("ret", idx).c_str()), ret_type.str);
		for (auto [idx, param_type]: enumerate(func_signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param_type.str);
		
		// instruction index, stack state, shadow stack state, pointer stack state, stack size, shadow size, pointer size
		std::vector<std::tuple<
			usize,
			decltype(type_size_stack),
			decltype(type_shadow_size_stack),
			decltype(type_shadow_pointer_size_stack),
			usize,
			usize,
			usize
		>> dfs_stack{
			{ ctx.function.body.size(), {}, {}, {}, 0, 0, 0 }  // sentinel
		};
		std::vector<bool> visited_instructions(ctx.function.body.size());
		usize             index = 0;

		while (index != ctx.function.body.size()) {
			if (visited_instructions[index]) {
				std::tie(
					index,
					type_size_stack,
					type_shadow_size_stack,
					type_shadow_pointer_size_stack,
					curr_stack_size,
					curr_shadow_size,
					curr_shadow_pointer_size
				) = dfs_stack.back();
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
						type_shadow_size_stack,
						type_shadow_pointer_size_stack,
						curr_stack_size,
						curr_shadow_size,
						curr_shadow_pointer_size
					);
				}
				instr_case(Op_jmpIfNot_label, instr) {
					index++;
					dfs_stack.emplace_back(
						label_positions[instr.label.label_name],
						type_size_stack,
						type_shadow_size_stack,
						type_shadow_pointer_size_stack,
						curr_stack_size,
						curr_shadow_size,
						curr_shadow_pointer_size
					);
				}
				instr_case(Op_ret, instr) {
					std::tie(
						index,
						type_size_stack,
						type_shadow_size_stack,
						type_shadow_pointer_size_stack,
						curr_stack_size,
						curr_shadow_size,
						curr_shadow_pointer_size
					) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_case(Op_call_func, instr) {
					usize number_of_params = program_ctx.function_forward_declarations
					                             .at(instr.function.function_name)
					                             ->signature.parameters.size();
					for (usize i = 0; i < number_of_params; i++) pop();
					index++;
				}
				instr_case(Op_call_builtinfunc, instr) {
					for (usize i = 0;
					     i < builtins::getBuiltinFunctionSignature(instr.function.function_name)
					             .value()
					             ->parameters.size();
					     i++) {
						pop();
					}
					index++;
				}
				instr_case(Op_call_cfunc, instr) {
					for (usize i = 0;
					     i < program_ctx.ext_c_functions.at(instr.function.function_name)
					             ->signature.parameters.size();
					     i++) {
						pop();
					}
					index++;
				}
				instr_case(Op_virtual_call_pptr_method, instr) {
					for (usize i = 0; i < *seek_method_param_count(instr.method.method_name); i++)
						pop();
					index++;
				}
				instr_case(Op_ret_tailcall_func, instr) {
					std::tie(
						index,
						type_size_stack,
						type_shadow_size_stack,
						type_shadow_pointer_size_stack,
						curr_stack_size,
						curr_shadow_size,
						curr_shadow_pointer_size
					) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_default { index++; }
			}
		}

		ctx.locals_map        = std::move(result);
		ctx.local_stack_size  = max_stack_size;
		ctx.local_block_count = max_block_count;

		ctx.shadow_data_offsets.assign(max_stack_size, 0);
		ctx.shadow_pointer_offsets.assign(max_stack_size, 0);
		for (const auto& [name, entry] : ctx.locals_map) {
			populateOffsets(
				ctx.shadow_data_offsets,
				ctx.shadow_pointer_offsets,
				entry.offset,
				entry.shadow_offset,
				entry.shadow_pointer_offset,
				entry.type
			);
		}

		ctx.block_shadow_data_offsets.assign(max_block_count, 0);
		ctx.block_shadow_pointer_offsets.assign(max_block_count, 0);
		for (const auto& [name, entry] : ctx.locals_map) {
			if (entry.block_idx < max_block_count) {
				ctx.block_shadow_data_offsets[entry.block_idx] = static_cast<u32>(entry.shadow_offset);
				ctx.block_shadow_pointer_offsets[entry.block_idx] = static_cast<u32>(entry.shadow_pointer_offset);
			}
		}
	}

	void Compiler::compileNewFunctions(const std::vector<code::Function>& new_functions) {
		// Forward declare all functions
		for (const auto& function: new_functions)
			program_ctx.function_forward_declarations.insert(function, function.name);

		for (const auto& function: new_functions) {
			FunctionCompilationContext ctx(function);
			calculateOffsets(ctx);

			// Calculate the functions metadata.
			code::FuncSignature   signature                = function.signature;
			usize                 parameters_size          = 0;
			usize                 parameters_shadow_size   = 0;
			usize                 parameters_pointer_size  = 0;
			std::vector<TypeCRef> parameters;
			parameters.reserve(signature.parameters.size());

			for (const auto& param: signature.parameters) {
				auto type = low_program.types->at(param.str);
				parameters.emplace_back(type);
				parameters_size += type->getSize().asInt();
				parameters_shadow_size += type->getShadowSize();
				parameters_pointer_size += type->getPointerSize();
			}

			low::MicroBytecode bytecode = lowerInstructions(ctx);

			u64                   ret_type_sum    = 0;
			u64                   ret_shadow_sum  = 0;
			u64                   ret_pointer_sum = 0;
			std::vector<TypeCRef> result_types    = {};
			for (auto& ret: signature.result_types) {
				auto type = low_program.types->at(ret);
				ret_type_sum += type->getSize().asInt();
				ret_shadow_sum += type->getShadowSize();
				ret_pointer_sum += type->getPointerSize();
				result_types.emplace_back(type);
			}

			low_program.functions.insert(
				low::LowFuncData{ .name                   = function.name,
			                      .bc                 = std::move(bytecode),
			                      .local_stack_size   = ctx.local_stack_size,
			                      .local_block_count  = ctx.local_block_count,
			                      .arg_size           = parameters_size,
			                      .ret_size           = ret_type_sum,
			                      .arg_shadow_size    = parameters_shadow_size,
			                      .arg_pointer_size   = parameters_pointer_size,
			                      .ret_shadow_size    = ret_shadow_sum,
			                      .ret_pointer_size   = ret_pointer_sum,
			                      .parameters         = std::move(parameters),
			                      .result_types       = std::move(result_types),
			                      .shadow_data_offsets = std::move(ctx.shadow_data_offsets),
			                      .shadow_pointer_offsets = std::move(ctx.shadow_pointer_offsets),
			                      .block_shadow_data_offsets = std::move(ctx.block_shadow_data_offsets),
			                      .block_shadow_pointer_offsets = std::move(ctx.block_shadow_pointer_offsets) },
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
				.type                         = low_program.types->at(global.type),
				.ctor_name                    = ctor_name,
				.dtor_name                    = dtor_name,
				.global_buffer_offset         = program_ctx.global_buffer_size.asInt(),
				.global_block_idx             = program_ctx.global_count,
				.global_shadow_data_offset    = program_ctx.global_shadow_buffer_size,
				.global_shadow_pointer_offset = program_ctx.global_pointer_buffer_size,
			};
			low_program.global_data.insert(data, global.name);

			program_ctx.global_count += 1;
			program_ctx.global_buffer_size += Bytes(data.type->getSize().asInt());
			program_ctx.global_shadow_buffer_size += data.type->getShadowSize();
			program_ctx.global_pointer_buffer_size += data.type->getPointerSize();
		}

		low_program.global_buffer_size         = program_ctx.global_buffer_size;
		low_program.global_count               = program_ctx.global_count;
		low_program.global_shadow_buffer_size  = program_ctx.global_shadow_buffer_size;
		low_program.global_shadow_pointer_size = program_ctx.global_pointer_buffer_size;

		low_program.global_shadow_data_offsets.assign(program_ctx.global_buffer_size.asInt(), 0);
		low_program.global_shadow_pointer_offsets.assign(program_ctx.global_buffer_size.asInt(), 0);
		low_program.global_block_shadow_data_offsets.assign(program_ctx.global_count, 0);
		low_program.global_block_shadow_pointer_offsets.assign(program_ctx.global_count, 0);

		for (const auto& global: new_globals) {
			auto name = global.name;
			auto data = low_program.global_data.at(name);
			populateOffsets(
				low_program.global_shadow_data_offsets,
				low_program.global_shadow_pointer_offsets,
				data->global_buffer_offset,
				data->global_shadow_data_offset,
				data->global_shadow_pointer_offset,
				data->type
			);

			if (data->global_block_idx < program_ctx.global_count) {
				low_program.global_block_shadow_data_offsets[data->global_block_idx] = static_cast<u32>(data->global_shadow_data_offset);
				low_program.global_block_shadow_pointer_offsets[data->global_block_idx] = static_cast<u32>(data->global_shadow_pointer_offset);
			}
		}
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
			auto param_size_sum = std::ranges::fold_left(
				params | std::views::transform([](const auto& param) {
					return param->getSize().asInt();
				}),
				0,
				std::plus()
			);

			std::vector<TypeCRef> rets = new_func.signature.result_types
			                           | std::views::transform([this](const auto& param_name) {
											 return low_program.types->at(param_name);
										 })
			                           | std::ranges::to<std::vector<TypeCRef>>();

			low_program.extern_c_functions.insert(
				low::LowExternCFunction{
					.name               = new_func.name,
					.function_pointer   = new_func.function_pointer,
					.parameter_size_sum = param_size_sum,
					.parameters         = std::move(params),
					.result_types       = std::move(rets),
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

	Compiler::Compiler(const api::ProcessSettings& settings) : settings_(settings) {}
}
