#include "safe_compiler.hpp"

#include "instruction_lowering.hpp"

#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/low_program/utils.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/loader/compiler/safe/type_builder.hpp>
#include <vm/utils/interpret.hpp>

namespace vm::loader::compiler::safe {

	static usize getIntTypeSize(const code::valid_type::TypeSize& size) {
		return static_cast<usize>(size.assumePointerSize(Bytes(16)));
	}

	namespace detail {

#define DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(FAMILY_CONCEPT, ...)                                   \
	template<FAMILY_CONCEPT ToType>                                                                  \
	struct LowerArgumentImpl<ToType> {                                                               \
		template<opargs::ArgumentType FromType>                                                      \
		static u64 lower(                                                                            \
			[[maybe_unused]] const SafeCompiler&                                       compiler,     \
			[[maybe_unused]] const vm::loader::compiler::detail::FunctionStackContext& stack_ctx,    \
			[[maybe_unused]] base::HashMap<base::StrID, usize>&                        label_id_map, \
			const FromType&                                                            opcode_arg,   \
			code::StackStateID stack_state_id                                                        \
		) {                                                                                          \
			__VA_ARGS__                                                                              \
		}                                                                                            \
	}

#define DEFINE_LOWER_ARGUMENT_IMPL(LOW_TO_TYPE, HIGH_FROM_TYPE, ...)                                 \
	template<>                                                                                       \
	struct LowerArgumentImpl<LOW_TO_TYPE> {                                                          \
		static u64 lower(                                                                            \
			[[maybe_unused]] const SafeCompiler&                                       compiler,     \
			[[maybe_unused]] const vm::loader::compiler::detail::FunctionStackContext& stack_ctx,    \
			[[maybe_unused]] base::HashMap<base::StrID, usize>&                        label_id_map, \
			const HIGH_FROM_TYPE&                                                      opcode_arg,   \
			[[maybe_unused]] code::StackStateID stack_state_id                                       \
		) {                                                                                          \
			__VA_ARGS__                                                                              \
		}                                                                                            \
	}
		// clang-format off

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::PlaceDataArgumentType,

			if_opt_some(opcode_arg.frame, frame_idx) {
				CORE_ASSERT(stack_ctx.thread_evaluating_expr.has_value(), "there should be a thread evaluating this expr");
				auto& thread = **stack_ctx.thread_evaluating_expr;

				auto [prev_func, stack_state] = *thread.getFuncAndStackStateOfThread(frame_idx);
				auto relative_offset = getIntTypeSize(*prev_func->local_stack.getByteOffset(stack_state, opcode_arg.var_name));
				
				usize stack_size = thread.getNumberOfCurrentStackFrames();

				auto prev_frame_base = thread.getStackFrame(frame_idx).local_stack;
				auto current_end = thread.getStackFrame(stack_size - 1).local_stack_head;
				
				usize frame_offset = usize(prev_frame_base - current_end);

				return (frame_offset + relative_offset) & (~(1ULL << 63));
			}

			if (auto&& maybe_offset = stack_ctx.function.local_stack.getByteOffset(stack_state_id, opcode_arg.var_name); maybe_offset.has_value()) {
				return getIntTypeSize(*maybe_offset);
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_buffer_offset | (1ULL << 63);
		);

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::PlaceBlockArgumentType,

			if_opt_some(opcode_arg.frame, frame_idx) {
				CORE_ASSERT(stack_ctx.thread_evaluating_expr.has_value(), "there is a thread evaluating this expr");
				auto& thread = **stack_ctx.thread_evaluating_expr;

				auto [prev_func, stack_state] = *thread.getFuncAndStackStateOfThread(frame_idx);
				auto relative_offset = *prev_func->local_stack.getIdx(stack_state, opcode_arg.var_name);
				
				usize stack_size = thread.getNumberOfCurrentStackFrames();

				auto prev_frame_base = thread.getStackFrame(frame_idx).local_block_ref_stack_base;
				auto current_end = thread.getStackFrame(stack_size - 1).local_block_ref_stack_end;
				
				usize frame_offset = usize(prev_frame_base - current_end);

				return (frame_offset + relative_offset) & (~(1ULL << 63));
			}

			if(auto maybe_val = stack_ctx.function.local_stack.getIdx(stack_state_id, opcode_arg.var_name)) {
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
		    return compiler.high_program.functions().idOf(opcode_arg.function_name).value();
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
			if (!label_id_map.contains(opcode_arg.label_name))
				label_id_map.put(opcode_arg.label_name, label_id_map.size());
			return label_id_map.at(opcode_arg.label_name);
		);
		// clang-format on

#undef DEFINE_LOWER_ARGUMENT_IMPL
#undef DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY

	}  // namespace detail

	template<opargs::ArgumentType FromType, low::opargs::ArgumentType ToType>
	u64 SafeCompiler::lowerArgument(
		const vm::loader::compiler::detail::FunctionStackContext& ctx,
		base::HashMap<base::StrID, usize>&                        label_id_map,
		const FromType&                                           opcode_arg,
		code::StackStateID                                        stack_state
	) const {
		return detail::LowerArgumentImpl<ToType>::lower(
			*this, ctx, label_id_map, opcode_arg, stack_state
		);
	}

	void SafeCompiler::linkLabelArguments(
		low::MicroBytecode& instructions, const base::HashMap<usize, usize>& label_map
	) const {
		for (auto [instr_idx, instr]: std::views::enumerate(instructions)) {
			auto       opcode_num = std::to_underlying(getInstructionOpcode(instr));
			std::array args{ Ref(&instr.arg0), Ref(&instr.arg1) };
			auto       are_args_labels = low::instruction_tags::IS_ARGUMENT_LABEL.at(opcode_num);

			for (auto [arg, is_label]: std::views::zip(args, are_args_labels))
				if (is_label) *arg = label_map.at(*arg) - static_cast<usize>(instr_idx) - 1;
		}
	}

	std::pair<low::MicroBytecode, std::vector<vm::low::LowFuncData::InstructionRange>> SafeCompiler::
		lowerInstructions(const vm::loader::compiler::detail::FunctionStackContext& ctx) const {
		detail::SafeMicroBytecodeBuilder                    builder{ *this, ctx };
		std::vector<vm::low::LowFuncData::InstructionRange> instruction_mapping;

		for (usize i = 0; i < ctx.function.body.size(); i++) {
			builder.curr_state     = ctx.function.stack_states[i];
			auto instruction_range = builder.add(ctx.function.body[i]);
			instruction_mapping.push_back(instruction_range);
		}

		auto [micro_bytecode, label_map] = builder.build();
		linkLabelArguments(micro_bytecode, label_map);

		return { std::move(micro_bytecode), std::move(instruction_mapping) };
	}

	void SafeCompiler::compileNewFunctions(
		const std::vector<code::valid_function::ValidFunction>& new_functions
	) {
		for (const auto& function: new_functions) {
			vm::loader::compiler::detail::FunctionStackContext ctx
				= calculateStackContext(function);

			usize new_func_id
				= low_program.functions.insert(lowerFunction(function, ctx), function.name);
			// This may look awkward, but it allows `LowFuncData` to know its own stable ID in the
			// map, which makes it possible to avoid hashmap lookups on function calls with JIT.
			low_program.functions[new_func_id].id = new_func_id;
		}
	}

	void SafeCompiler::compileNewGlobals(const std::vector<code::GlobalData>& new_globals) {
		for (const auto& global: new_globals) {
			auto global_init = [&] -> low::GlobalInit {
				if_opt_some(global.initial_value, initial_value) {
					// It's a bummer we have to copy here...
					// @TODO: #1306 Think if we can avoid copying here
					return low::GlobalInitialValue{ initial_value };
				}
				return low::GlobalCtorDtor{
					.ctor_name = global.ctor_name.map([](auto ident) { return ident.str; }),
					.dtor_name = global.dtor_name.map([](auto ident) { return ident.str; })
				};
			}();


			low::LowGlobalData data{
				.type                 = low_program.types->at(global.type),
				.init                 = global_init,
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

	CRef<vm::low::LowVMProgram> SafeCompiler::getLowProgram() const { return &low_program; }

	ProgramSize SafeCompiler::getCurrentProgramSize() const {
		return { .function_count       = low_program.functions.size(),
			     .global_count         = low_program.global_data.size(),
			     .type_count           = low_program.types->size(),
			     .ext_c_function_count = low_program.extern_c_functions.size() };
	}

	void SafeCompiler::compileNewTypes(const std::vector<code::valid_type::ValidType>& new_types) {
		if (std::ranges::empty(new_types)) return;

		vm::code::detail::rebuildTypeMetadata(low_program.types.refMut(), high_program.types());

		// Update method ID to name maps, since new methods may have appeared after new types were
		// added.
		for (const auto& new_type: new_types) {
			auto type_from_metadata = low_program.types->at(new_type.getName());
			if_opt_some(type_from_metadata->getInheritanceMetadata(), metadata) {
				//@TODO: #962 https://github.com/ducktype-org/duckling/issues/962
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

	void SafeCompiler::compileNewExtCFunctions(
		const std::vector<code::ExternalCFunction>& new_functions
	) {
		for (const auto& new_func: new_functions) {
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

	std::expected<vm::loader::FatBytecodePosition, vm::loader::MappingException> SafeCompiler::
		mapLowVMProgramPositionToCodeCollectionPosition(vm::low::LowCodePosition position) const {
		auto& mapping = position.function->instruction_mapping;

		// We need to find the first instruction range that starts after the given instruction
		// index, then check if the previous one contains it
		auto it = std::ranges::upper_bound(
			mapping,
			vm::low::LowFuncData::InstructionRange{
				.begin = position.instruction_index,
				.end   = std::numeric_limits<usize>::max(),
			}
		);

		if (it == mapping.begin()) return std::unexpected(MappingException::MissingMapping);

		auto candidate = it - 1;
		if (!candidate->contains(position.instruction_index))
			return std::unexpected(MappingException::MissingMapping);

		return FatBytecodePosition{
			.function_name     = position.function->name,
			.instruction_index = usize(candidate - mapping.begin()),
		};
	}

	vm::low::LowFuncData SafeCompiler::lowerExpr(
		const code::valid_function::ValidFunction& function, CRef<SafeVMThread> thread
	) const {
		vm::loader::compiler::detail::FunctionStackContext ctx = calculateStackContext(function);
		ctx.thread_evaluating_expr                             = thread;

		return lowerFunction(function, ctx);
	}

	vm::low::LowFuncData SafeCompiler::lowerFunction(
		const code::valid_function::ValidFunction&                function,
		const vm::loader::compiler::detail::FunctionStackContext& ctx
	) const {
		auto [bytecode, instruction_mapping] = lowerInstructions(ctx);

		// Calculate the functions metadata.
		code::FuncSignature        signature       = function.signature;
		code::valid_type::TypeSize parameters_size = {};
		std::vector<TypeCRef>      parameters;
		parameters.reserve(signature.parameters.size());

		for (const auto& param: signature.parameters) {
			CRef<code::valid_type::ValidType> type
				= high_program.getTypeContext().getCurrentTypes().at(param.str);
			parameters.emplace_back(low_program.getTypes().at(type->getName()));
			parameters_size += type->getSize();
		}

		code::valid_type::TypeSize ret_type_sum = {};
		std::vector<TypeCRef>      result_types;
		parameters.reserve(signature.result_types.size());

		for (auto& ret: signature.result_types) {
			ret_type_sum += high_program.getTypeContext().getCurrentTypes().at(ret)->getSize();
			result_types.emplace_back(low_program.types->at(ret));
		}

		return low::LowFuncData{
			.name      = function.name,
			.id        = 0,  // placeholder, replaced if function is not a volatile expression
			.high_func = &function,
#ifdef ENABLE_JIT
			.cfg = vm::low::cf::ControlFlowGraph(bytecode),
#endif
			.bc                  = std::move(bytecode),
			.local_stack_size    = getIntTypeSize(ctx.local_stack_size),
			.local_block_count   = ctx.local_block_count,
			.arg_size            = getIntTypeSize(parameters_size),
			.ret_size            = getIntTypeSize(ret_type_sum),
			.parameters          = std::move(parameters),
			.result_types        = std::move(result_types),
			.instruction_mapping = std::move(instruction_mapping),
		};
	}

}
