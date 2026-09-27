#include "safe_compiler.hpp"

#include "instruction_lowering.hpp"

#include <vm/bytecode/validator/ffi_type_builder.hpp>
#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/low_program/utils.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/loader/compiler/safe/type_builder.hpp>
#include <vm/utils/interpret.hpp>

#ifdef ENABLE_JIT
	#include <vm/core/safe/low_program/cfg/cf_analysis.hpp>
	#include <vm/core/safe/low_program/instruction.hpp>
#endif

namespace vm::loader::compiler::safe {

	namespace detail {

#define DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(FAMILY_CONCEPT, ...)                                   \
	template<FAMILY_CONCEPT ToType>                                                                  \
	struct LowerArgumentImpl<ToType> final {                                                         \
		template<opargs::ArgumentType FromType>                                                      \
		static u64 lower(                                                                            \
			[[maybe_unused]] const SafeCompiler&                                       compiler,     \
			[[maybe_unused]] const vm::loader::compiler::detail::FunctionStackContext& stack_ctx,    \
			[[maybe_unused]] base::HashMap<base::StrID, usize>&                        label_id_map, \
			const FromType&                                                            opcode_arg,   \
			[[maybe_unused]] code::StackStateID stack_state_id                                       \
		) {                                                                                          \
			__VA_ARGS__                                                                              \
		}                                                                                            \
	}

#define DEFINE_LOWER_ARGUMENT_IMPL(LOW_TO_TYPE, HIGH_FROM_TYPE, ...)                                 \
	template<>                                                                                       \
	struct LowerArgumentImpl<LOW_TO_TYPE> final {                                                    \
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

			if_opt_some (opcode_arg.frame, frame_idx) {
				auto& expr_mode = std::get<vm::code::detail::Expr>(stack_ctx.mode);
				auto& frame = expr_mode.call_stack_base[frame_idx];

				auto relative_offset = getIntTypeSize(*expr_mode.getUpcomingHighPosition(frame_idx)->getByteOffset(opcode_arg.var_name));
				auto prev_frame_base = frame.local_stack;
				auto stack_base = expr_mode.call_stack_base->local_stack;
				
				usize frame_offset = usize(prev_frame_base - stack_base);

				usize final_offset = (frame_offset + relative_offset);
				CORE_ASSERT ((final_offset & (1ULL << 63)) == 0, "final offset should have top bit off");

				return final_offset;
			}

			if (auto&& maybe_offset = stack_ctx.function.local_stack.getByteOffset(stack_state_id, opcode_arg.var_name); maybe_offset.has_value()) {
				return getIntTypeSize(*maybe_offset);
			}
			return compiler.low_program.getGlobals().at(opcode_arg.var_name)->global_buffer_offset | (1ULL << 63);
		);

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			low::opargs::PlaceBlockArgumentType,

			if_opt_some (opcode_arg.frame, frame_idx) {
				auto& expr_mode = std::get<vm::code::detail::Expr>(stack_ctx.mode);
				auto& frame = expr_mode.call_stack_base[frame_idx];

				auto relative_offset = *expr_mode.getUpcomingHighPosition(frame_idx)->getBlockIdx(opcode_arg.var_name);
				auto prev_frame_base = frame.local_slot_stack_base;
				auto stack_base = expr_mode.call_stack_base->local_slot_stack_base;
				
				usize frame_offset = usize(prev_frame_base - stack_base);

				usize final_offset = (frame_offset + relative_offset);
				CORE_ASSERT ((final_offset & (1ULL << 63)) == 0, "final offset should have top bit off");

				return final_offset;
			}

			if(auto maybe_val = stack_ctx.function.local_stack.getBlockIdx(stack_state_id, opcode_arg.var_name)) {
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
			low::opargs::FFIFunction,
			opargs::FFIFunctionName,
			return safeReadObjectBytes<u64>(compiler.low_program.getFFIFunctions().at(opcode_arg.function_name));
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

		DEFINE_LOWER_ARGUMENT_IMPL_FOR_FAMILY(
			std::same_as<low::opargs::VMValPtr>,

			if constexpr (std::same_as<FromType, opargs::VMValueIdentifier>) {
				auto& expr_mode = std::get<vm::code::detail::Expr>(stack_ctx.mode);
				return std::bit_cast<u64>(expr_mode.vm_values->at(opcode_arg.id).get());
			} else {
				return std::bit_cast<u64>(opcode_arg.ptr);
			}
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

	std::expected<void, std::string> SafeCompiler::setBreakpoint(
		const base::StrID& func_name, usize idx, bool enable
	) {
		auto maybe_function = low_program.functions.atMaybe(func_name);
		if (!maybe_function) return std::unexpected{ "setBreakpoint: Function does not exist" };
		auto& function = **maybe_function;

		return function.setBreakpoint(idx, enable);
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

	SafeCompiler::LoweredFunction SafeCompiler::lowerInstructions(
		const vm::loader::compiler::detail::FunctionStackContext& ctx
	) const {
		detail::SafeMicroBytecodeBuilder                    builder{ *this, ctx };
		std::vector<vm::low::LowFuncData::InstructionRange> instruction_mapping;

		for (usize i = 0; i < ctx.function.body.size(); i++) {
			const auto& instruction = ctx.function.body[i];
			builder.curr_state      = ctx.function.stack_states[i];

			// `init` pushes a new variable on top of the stack, but its recorded state is the one
			// before the push. Ask about the variable at the state after the instruction, where
			// it already exists.
			using namespace code::instructions;
			const auto opcode = instruction.opcode();
			if (opcode == Op_init_pany_type::OPCODE
			    || opcode == Op_initFromVMValue_pany_idvmval::OPCODE
			    || opcode == Op_initFromVMValue_pany_immvmval::OPCODE) {
				CORE_ASSERT(
					i + 1 < ctx.function.stack_states.size(),
					"function can't end with the initialization instruction"
				);
				builder.curr_state = ctx.function.stack_states[i + 1];
			}

			auto instruction_range = builder.add(instruction);
			instruction_mapping.push_back(instruction_range);
		}

		auto [micro_bytecode, label_map] = builder.build();
		linkLabelArguments(micro_bytecode, label_map);

		return { .bytecode            = std::move(micro_bytecode),
			     .instruction_mapping = std::move(instruction_mapping) };
	}

	void SafeCompiler::compileNewFunctions(
		const std::vector<CRef<code::valid_function::ValidFunction>>& new_functions
	) {
		for (const auto& func_ref: new_functions) {
			const auto&                                        function = *func_ref;
			vm::loader::compiler::detail::FunctionStackContext ctx
				= calculateStackContext(function);

			usize new_func_id
				= low_program.functions.insert(lowerFunction(function, ctx), function.name);
			// This may look awkward, but it allows `LowFuncData` to know its own stable ID in the
			// map, which makes it possible to avoid hashmap lookups on function calls with JIT.
			low_program.functions[new_func_id].id = new_func_id;

#ifdef ENABLE_JIT
			if (jit_enabled) {
				auto& func    = low_program.functions[new_func_id];
				func.jit_data = jit::JitFuncData(func.bc, func.jit_func_entrypoint_offset);
				// Patch entrypoint opcodes into bc; orig_bc stays pristine.
				for (usize i = 0; i < func.jit_data.cfgs.size(); i++) {
					if (func.jit_data.cfgs[i].empty()) continue;  // Not an entrypoint
					auto maybe_old_opcode = func.replaceOpcode(
						i,
						i == func.jit_func_entrypoint_offset ? low::MicroOpcode::jitFuncEntrypoint
															 : low::MicroOpcode::jitLoopEntrypoint
					);
					CORE_ASSERT(maybe_old_opcode.has_value(), "JIT entrypoint offset out of bounds");
				}
			}
#endif
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
			     .ext_c_function_count = low_program.extern_c_functions.size(),
			     .ffi_function_count   = low_program.ffi_functions.size() };
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

	void SafeCompiler::compileNewFFIFunctions(const std::vector<code::FFIFunction>& new_functions) {
		const auto& high_types = high_program.getTypeContext().getCurrentTypes();

		for (const auto& new_func: new_functions) {
			low::LowFFIFunction low_func;
			low_func.name   = new_func.name;
			low_func.symbol = new_func.symbol;

			code::ffi_detail::FFITypeStorage storage;

			for (const auto& param: new_func.signature.parameters) {
				CRef<code::valid_type::ValidType> type = high_types.at(param.str);
				low_func.parameters.emplace_back(low_program.types->at(type->getName()));
				low_func.ffi_arg_types.push_back(
					code::ffi_detail::buildFFIType(*type, high_types, storage)
				);
			}

			ffi_type* result_type = &ffi_type_void;
			for (const auto& ret: new_func.signature.result_types) {
				CRef<code::valid_type::ValidType> type = high_types.at(ret.str);
				low_func.result_types.emplace_back(low_program.types->at(type->getName()));
				result_type = code::ffi_detail::buildFFIType(*type, high_types, storage);
			}

			low_func.struct_types    = std::move(storage.struct_types);
			low_func.struct_elements = std::move(storage.struct_elements);

			// The cif captures pointers into `ffi_arg_types` and `struct_types` - moving the
			// whole object afterwards is fine, as those live on the heap.
			// Kept out of CORE_ASSERT: its condition is not evaluated in release builds.
			[[maybe_unused]] ffi_status status = ffi_prep_cif(
				&low_func.cif,
				FFI_DEFAULT_ABI,
				base::safeIntConv<unsigned>(low_func.ffi_arg_types.size()),
				result_type,
				low_func.ffi_arg_types.data()
			);
			CORE_ASSERT(status == FFI_OK, "ffi_prep_cif failed for FFI function");

			low_program.ffi_functions.insert(std::move(low_func), new_func.name);
		}
	}

	vm::low::LowFuncData SafeCompiler::lowerExpr(
		const code::valid_function::ValidFunction& function,
		const vm::code::detail::ValidationMode&    mode
	) const {
		vm::loader::compiler::detail::FunctionStackContext ctx = calculateStackContext(function);
		ctx.mode                                               = mode;

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
		result_types.reserve(signature.result_types.size());

		for (auto& ret: signature.result_types) {
			ret_type_sum += high_program.getTypeContext().getCurrentTypes().at(ret)->getSize();
			result_types.emplace_back(low_program.types->at(ret));
		}

#ifdef ENABLE_JIT
		// Keep the function entrypoint pristine until JIT compilation patches its bytecode.
		const usize function_jit_entrypoint = low::cf::functionEntrypointOffset(bytecode);
		bytecode[function_jit_entrypoint]   = makeLowInstruction(low::MicroOpcode::nop, 0, 0);
#endif

		low::LowFuncData func(&function);
		func.name = function.name;
		func.id   = low::LowFuncData::NO_FUNCTION_ID;
#ifdef ENABLE_JIT
		func.jit_func_entrypoint_offset = function_jit_entrypoint;
#endif
		// we need two copies of the bytecode
		func.bc                  = bytecode;
		func.orig_bc             = std::move(bytecode);
		func.local_stack_size    = getIntTypeSize(ctx.local_stack_size);
		func.local_slot_count    = ctx.local_slot_count;
		func.arg_size            = getIntTypeSize(parameters_size);
		func.ret_size            = getIntTypeSize(ret_type_sum);
		func.parameters          = std::move(parameters);
		func.result_types        = std::move(result_types);
		func.instruction_mapping = std::move(instruction_mapping);

		return func;
	}
}
