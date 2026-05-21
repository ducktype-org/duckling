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
			if_opt_some(ctx.function.local_stack->getByteOffset(ctx.curr_state, opcode_arg.var_name), offset) {
				return offset.assumePointerSize(ctx.pointer_size).asInt();
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_buffer_offset | (1ULL << 63);
		);

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::PlaceBlockArgumentType,
			if(auto maybe_val = ctx.function.local_stack->getIdx(ctx.curr_state, opcode_arg.var_name)) {
				return *maybe_val;
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

		for (auto& instr: ctx.function.body) {
			ctx.curr_state         = instr.visit([](auto&& i) { return i.stack_state; });
			auto instruction_range = builder.add(instr);
			ctx.instruction_mapping.push_back(instruction_range);
		}

		auto [micro_bytecode, label_map] = builder.build();
		linkLabelArguments(micro_bytecode, label_map);

		return micro_bytecode;
	}

	void Compiler::calculateOffsets(FunctionCompilationContext& ctx) {
		usize max_stack_size  = 0;
		usize max_block_count = 0;

		CORE_ASSERT(ctx.function.local_stack, "A given function should have passed the validation");
		auto& db = *ctx.function.local_stack;

		for (auto instr: ctx.function.body) {
			auto state = instr.visit([&](auto&& i) { return i.stack_state; });

			max_block_count = std::max(max_block_count, db.size(state));
			max_stack_size  = std::max(
                max_stack_size, db.byteSize(state).assumePointerSize(ctx.pointer_size).asInt()
            );
		}

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
			code::FuncSignature   signature       = function.signature;
			usize                 parameters_size = 0;
			std::vector<TypeCRef> parameters;
			parameters.reserve(signature.parameters.size());

			for (const auto& param: signature.parameters) {
				auto type = low_program.types->at(param.str);
				parameters.emplace_back(type);
				parameters_size += type->getSize().asInt();
			}

			low::MicroBytecode bytecode = lowerInstructions(ctx);

			u64                   ret_type_sum = 0;
			std::vector<TypeCRef> result_types = {};
			for (auto& ret: signature.result_types) {
				ret_type_sum += low_program.types->at(ret)->getSize().asInt();
				result_types.emplace_back(low_program.types->at(ret));
			}

			low_program.functions.insert(
				low::LowFuncData{
					.name                = function.name,
					.bc                  = std::move(bytecode),
					.local_stack_size    = ctx.local_stack_size,
					.local_block_count   = ctx.local_block_count,
					.arg_size            = parameters_size,
					.ret_size            = ret_type_sum,
					.parameters          = std::move(parameters),
					.result_types        = std::move(result_types),
					.instruction_mapping = std::move(ctx.instruction_mapping),
				},
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
				.type                 = low_program.types->at(global.type),
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

}
