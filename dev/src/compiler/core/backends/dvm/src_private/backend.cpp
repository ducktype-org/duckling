#include <backends/dvm/backend.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <query_framework/context.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <algorithm>
#include <iostream>
#include <ranges>
#include <variant>

#define INVALID_CASE(tp, reason)                                                    \
	variant_case(tp, _) {                                                           \
		CORE_PANIC("During handling of type ", base::typeName<tp>(), ": ", reason); \
	}

#define NOIMPL_CASE(tp, reason)                                              \
	variant_case(tp, _) {                                                    \
		throw base::NotYetImplemented(                                       \
			base::strConcat("Type ", base::typeName<tp>(), " for: ", reason) \
		);                                                                   \
	}


using namespace vm::code;
using namespace vm::code::builders;

namespace {
	void pushInstruction(vm::code::Function& function, const Instruction& instruction) {
		function.body.push_back(instruction);
	}

	void pushInstruction(vm::code::Function& function, const InstructionBuilder& builder) {
		for (const auto& instruction: builder.build()) pushInstruction(function, instruction);
	}
}

namespace compiler::backend_vm {
	namespace {
		vm::code::TypeOfData getTypeFromLayout(const tsl::TypeLayout& layout) {
			variant_match(layout()) {
				variant_case_novalue(tsl::EmptyTypeLayout) {
					return vm::code::PrimitiveType(base::StrID("void"), 0);
				}
				variant_case_novalue(tsl::IntegralTypeLayout) {
					auto bits = usize(layout.getSize());
					if (bits == 1) bits = 8;  // Boolean case.
					if (bits % 8 != 0) CORE_PANIC("Integral type size not divisible by 8");
					usize       bytes = bits / 8;
					std::string name  = "i" + std::to_string(bits);

					return vm::code::PrimitiveType(base::StrID(name.c_str()), bytes);
				}
				variant_case_novalue(tsl::FloatTypeLayout) {
					auto bits = usize(layout.getSize());
					CORE_ASSERT(
						bits == 16 || bits == 32 || bits == 64 || bits == 80,
						"Invalid size of float: ",
						bits
					);
					usize       bytes = bits / 8;
					std::string name  = "f" + std::to_string(bits);

					return vm::code::PrimitiveType(base::StrID(name.c_str()), bytes);
				}
				variant_default {
					CORE_PANIC(
						base::strConcat("Type not handled yet: ", layout.toStringIdentification())
					);
				}
			}
			CORE_UNREACHABLE();
		}

		vm::code::FunctionType getFunctionTypeFromLayouts(
			base::StrID                         function_name,
			const std::vector<tsl::TypeLayout>& parameter_layouts,
			const tsl::TypeLayout&              return_type_layout
		) {
			auto param_types = parameter_layouts | std::views::transform([](auto&& layout) {
								   return getTypeFromLayout(layout);
							   });

			auto param_type_names
				= param_types | std::views::transform([](auto&& type) { return typeName(type); })
			    | std::ranges::to<std::vector>();
			auto result_type      = getTypeFromLayout(return_type_layout);
			auto result_type_name = typeName(result_type);
			return vm::code::FunctionType{ function_name, param_type_names, result_type_name };
		}

		struct AddLirFuncContext final {
			query::Context&     ctx;
			CRef<lir::Function> lir_func;
			Function            bytecode_func;

			base::HashMap<usize, base::StrID>     block_id_to_label;
			base::Map<lir::LocalRef, base::StrID> lir_local_to_name;
			base::Map<lir::LocalRef, TypeOfData>  lir_local_types;

			// Used to create unique names for temporary values.
			usize                                        next_call_id = 0;
			base::Map<lir::LocalRef, u64>                variable_to_id;
			base::Map<lir::BlockRef, u64>                block_to_id;
			const base::HashMap<base::StrID, TypeOfData> TYPE_OF_DATA;

			AddLirFuncContext(
				query::Context&                           ctx,
				CRef<lir::Function>                       lir_function,
				const vm::StableObjIdNameMap<TypeOfData>& type_map
			):
				  ctx(ctx),
				  lir_func(lir_function),
				  bytecode_func(Function({}, lir_function->mangled_name, {})),
				  TYPE_OF_DATA([&type_map] {
					  base::HashMap<base::StrID, TypeOfData> map;
					  for (auto&& type: type_map) map.put(typeName(type), type);
					  return map;
				  }()) {
				variable_to_id = lir_function->getLocalVariableIDs();
				block_to_id    = lir_function->getBlockIDs();
			}
		};

		/**
		 * @brief Generate `init_lany_type` instruction.
		 */
		void initType(AddLirFuncContext& ctx, base::StrID variable_name, base::StrID type_name) {
			pushInstruction(
				ctx.bytecode_func, instructions::Op_init_lany_type(variable_name, type_name)
			);
		}

		void initLocals(AddLirFuncContext& ctx) {
			CORE_ASSERT(
				std::holds_alternative<FunctionType>(ctx.TYPE_OF_DATA.at(ctx.lir_func->mangled_name)
			    ),
				"Type not functional"
			);
			auto func_type_tod
				= std::get<FunctionType>(ctx.TYPE_OF_DATA.at(ctx.lir_func->mangled_name));

			// Save locals offset
			for (auto& var: ctx.lir_func->local_list) {
				// This is most likely redundant
				CORE_ASSERT(!ctx.lir_local_to_name.contains(&var), "Duplicated lir local");

				auto vm_type = getTypeFromLayout(var.layout);
				ctx.lir_local_types.put(&var, vm_type);
				match_optional(var.parameter_index) {
					opt_some(param_idx) {
						// In this case we are handling a parameter
						CORE_ASSERT(
							vm_type == ctx.TYPE_OF_DATA.at(func_type_tod.parameters.at(param_idx)),
							"getTypeFromLayout created an invalid type..."
						);

						auto name = base::StrID(base::strConcat("arg", param_idx).c_str());
						ctx.lir_local_to_name.put(&var, name);
					}
					opt_none {
						// In this case we are handling a regular variable
						auto tp_name = typeName(vm_type);
						auto var_name
							= base::StrID(base::strConcat("var", ctx.variable_to_id[&var]).c_str());
						initType(ctx, var_name, tp_name);
						ctx.lir_local_to_name.put(&var, var_name);
					}
				}
			}
		}

		/**
		 * @brief Inserts function signature type and types used by the signature.
		 */
		void insertFunctionSignatureType(
			std::vector<TypeOfData>& types, CRef<lir::Function> lir_function
		) {
			auto param_types
				= lir_function->parameter_layouts
			    | std::views::transform([](auto&& layout) { return getTypeFromLayout(layout); });

			std::ranges::for_each(param_types, [&](auto&& type) { types.push_back(type); });

			auto result_type = getTypeFromLayout(lir_function->return_type_layout);
			types.push_back(result_type);

			if (lir_function->mangled_name != "main") {
				types.emplace_back(getFunctionTypeFromLayouts(
					lir_function->mangled_name,
					lir_function->parameter_layouts,
					lir_function->return_type_layout
				));
			}
		}

		/**
		 * @brief Inserts types used by function's local variables.
		 */
		void insertFunctionLocalTypes(
			std::vector<TypeOfData>& types, CRef<lir::Function> lir_function
		) {
			auto local_layouts = lir_function->local_list
			                   | std::views::transform([](auto& local) { return local.layout; });

			for (const auto& layout: local_layouts) types.push_back(getTypeFromLayout(layout));
		}

		/**
		 * @brief Insert function types for all functions that are called somewhere in the LIR
		 * code. Used to add function types for functions that are not in the current module.
		 */
		void insertCalledFunctionTypes(
			std::vector<TypeOfData>& types, CRef<lir::Function> lir_function
		) {
			for (const auto& lir_block: lir_function->block_order) {
				for (const auto& lir_instruction: lir_block->instructions) {
					if (lir_instruction.operation == lir::Operation::Call) {
						auto func_literal
							= lir_instruction.arguments.at(0).get<lir::FunctionLiteral>();
						types.emplace_back(getFunctionTypeFromLayouts(
							func_literal.mangled_name,
							*func_literal.parameter_layouts,
							*func_literal.return_type_layout
						));
					}
				}
			}
		}

		void insertTypesUsedByFunction(
			std::vector<TypeOfData>& types, CRef<lir::Function> lir_function
		) {
			insertFunctionSignatureType(types, lir_function);
			insertFunctionLocalTypes(types, lir_function);
			insertCalledFunctionTypes(types, lir_function);
		}

		void registerBlock(AddLirFuncContext& ctx, lir::BlockRef block) {
			auto id         = ctx.block_to_id.at(block);
			auto label_name = base::strConcat("label_", id);
			ctx.block_id_to_label.put(id, base::StrID(label_name.data()));
		}

		void addBlockLabel(AddLirFuncContext& ctx, lir::BlockRef block) {
			auto id = ctx.block_to_id.at(block);
			pushInstruction(ctx.bytecode_func, instructions::Op_label{ ctx.block_id_to_label[id] });
		}

		vm::opargs::OpCodeArg outputToOpArg(
			vm::code::TypeOfData type, base::StrID name, bool is_global = false
		) {
			variant_match(type) {
				variant_case(vm::code::PrimitiveType, primitive) {
					if (primitive.size != 8 && primitive.size != 4 && primitive.size != 2
					    && primitive.size != 1)
						throw base::NotYetImplemented(base::strConcat(
							"Primitives of sizes different than 64 | 32 | 16 | 8 bits are not "
							"supported YET, name: ",
							primitive.name,
							", size: ",
							primitive.size
						));
					if (is_global) {
						if (primitive.size == 8) return vm::opargs::Global64{ name };
						if (primitive.size == 4) return vm::opargs::Global32{ name };
						if (primitive.size == 2) return vm::opargs::Global16{ name };
						if (primitive.size == 1) return vm::opargs::Global8{ name };
					} else {
						if (primitive.size == 8) return vm::opargs::StackLocal64{ name };
						if (primitive.size == 4) return vm::opargs::StackLocal32{ name };
						if (primitive.size == 2) return vm::opargs::StackLocal16{ name };
						if (primitive.size == 1) return vm::opargs::StackLocal8{ name };
					}
				}
				variant_case(vm::code::PointerType, pointer) {
					if (is_global)
						return vm::opargs::GlobalPtr(name);
					else
						return vm::opargs::StackLocalPtr(name);
				}

				INVALID_CASE(vm::code::FixedSizeTableType, "output target");
				INVALID_CASE(vm::code::DynamicTableType, "output target");
				INVALID_CASE(vm::code::DataType, "output target");
				INVALID_CASE(vm::code::VariantType, "output target");
				INVALID_CASE(vm::code::FunctionType, "output target");
			}
			CORE_UNREACHABLE();
		}

		vm::opargs::OpCodeArg lirOutputToOpArg(
			AddLirFuncContext& ctx, const lir::Instruction& lir_instruction
		) {
			variant_match(lir_instruction.output.value()) {
				variant_case(lir::LocalRef, local) {
					auto&& var_type = ctx.lir_local_types[local];
					return outputToOpArg(var_type, ctx.lir_local_to_name[local]);
				}
				variant_case(lir::LirGlobal, global) {
					auto vm_type = getTypeFromLayout(*global.layout);
					return outputToOpArg(vm_type, global.mangled_name, true);
				}
			}
			CORE_UNREACHABLE();
		}

		constexpr vm::opargs::OpCodeArg lirValueToOpArg(
			AddLirFuncContext& ctx, const lir::LIRValue& lir_value
		) {
			variant_match(lir_value.getVariant()) {
				variant_case(i64, value) return vm::opargs::Immediate{ value };
				variant_case(bool, value) return vm::opargs::Immediate{ value };
				variant_case(lir::LocalRef, local_ref) {
					auto&& var_type = ctx.lir_local_types[local_ref];
					return outputToOpArg(var_type, ctx.lir_local_to_name[local_ref]);
				}
				variant_case(lir::BlockRef, block_ref) {
					return vm::opargs::Label{ ctx.block_id_to_label[ctx.block_to_id[block_ref]] };
				}
				variant_case(lir::FunctionLiteral, function) {
					return vm::opargs::FunctionName(function.mangled_name);
				}
				variant_case(lir::LirGlobal, global) {
					auto vm_type = getTypeFromLayout(*global.layout);
					return outputToOpArg(vm_type, global.mangled_name, true);
				}
				variant_default { CORE_PANIC("Unhandled value case"); }
			}
			CORE_UNREACHABLE();
		}

		constexpr vm::opargs::OpCodeArg modifyVarNameOpArg(
			const vm::opargs::OpCodeArg& op_arg, base::StrID new_name
		) {
			variant_match(op_arg) {
#define LOCAL_CASE(type)                         \
	variant_case(vm::opargs::type, local_type) { \
		auto copy     = local_type;              \
		copy.var_name = new_name;                \
		return copy;                             \
	}
				FOR_EACH(LOCAL_CASE, VM_OPARG_LOCAL_TYPES);
#undef LOCAL_CASE
				variant_default { CORE_PANIC("Given op_arg is not a local argument"); }
			}

			CORE_UNREACHABLE();
		}

		constexpr vm::code::builders::OpKind lirOpToOpKind(lir::Operation operation) {
			using namespace vm::code::builders;
			switch (operation) {
			case lir::Operation::Assign:
				return OpKind::mov;
			case lir::Operation::IntegerAdd:
				return OpKind::add;
			case lir::Operation::IntegerSub:
				return OpKind::sub;
			case lir::Operation::IntegerMul:
				return OpKind::mul;
			case lir::Operation::IntegerUDiv:
				throw base::NotYetImplemented(base::enumToStr(operation).str());
			case lir::Operation::IntegerSDiv:
				return OpKind::div;
			case lir::Operation::IntegerUMod:
				throw base::NotYetImplemented(base::enumToStr(operation).str());
			case lir::Operation::IntegerSMod:
				return OpKind::mod;
			case lir::Operation::IntegerNeg:
				return OpKind::neg;
			case lir::Operation::Call:
				return OpKind::call;
			case lir::Operation::IntegerULt:
				throw base::NotYetImplemented(base::enumToStr(operation).str());
			case lir::Operation::IntegerSLt:
				throw base::NotYetImplemented(base::enumToStr(operation).str());
			default:
				CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
			}
			CORE_UNREACHABLE();
		}

		constexpr vm::code::builders::OpKind lirTerminatorToOpKind(lir::Operation terminator) {
			using namespace vm::code::builders;
			switch (terminator) {
			case lir::Operation::Jump:
				return OpKind::jmp;
			case lir::Operation::ReturnVoid:
				return OpKind::ret;
			case lir::Operation::ReturnValue:
				return OpKind::ret;
			case lir::Operation::Branch:
			default:
				CORE_PANIC("Invalid terminator: ", base::enumToStr(terminator));
			}
			CORE_UNREACHABLE();
		}

		void handleAddCall(AddLirFuncContext& ctx, std::deque<vm::opargs::OpCodeArg> args) {
			auto lir_result_argument = args.front();
			args.pop_front();
			auto called_func_arg = args.front();
			args.pop_front();

			base::StrID called_func_name
				= std::get<vm::opargs::FunctionName>(called_func_arg).function_name;
			auto called_func = std::get<FunctionType>(ctx.TYPE_OF_DATA.at(called_func_name));

			// Init result type

			usize call_id     = ctx.next_call_id++;
			auto  result_name = base::StrID(base::strConcat("call", call_id, "_res").c_str());
			auto  func_result_argument = modifyVarNameOpArg(lir_result_argument, result_name);
			initType(ctx, result_name, called_func.result);

			// Instantiate function parameters on the stack.

			for (const auto& [arg_id, op_arg, type_name]:
			     std::views::zip(std::views::iota(0), args, called_func.parameters)) {
				std::cerr << "Initializing: " << type_name.str() << '\n';
				auto arg_name
					= base::StrID(base::strConcat("call", call_id, "_arg", arg_id).c_str());
				initType(ctx, arg_name, type_name);

				InstructionBuilder mov_arg(OpKind::mov);
				mov_arg.pushArg(outputToOpArg(ctx.TYPE_OF_DATA[type_name], arg_name));
				mov_arg.pushArg(op_arg);
				pushInstruction(ctx.bytecode_func, mov_arg);
			}

			pushInstruction(ctx.bytecode_func, { OpKind::call, called_func_arg });

			pushInstruction(
				ctx.bytecode_func,
				{
					OpKind::mov,
					lir_result_argument,
					func_result_argument,
				}
			);
			pushInstruction(
				ctx.bytecode_func, { instructions::Op_deinit() }
			);  // Deinit func result
		}

		void addLirInstruction(AddLirFuncContext& ctx, const lir::Instruction& lir_instruction) {
			// Insert a comment about operation type.
			// @TODO: Improve this to contain more information.
			pushInstruction(
				ctx.bytecode_func,
				vm::code::instructions::Comment(base::StrID(
					base::strConcat("Operation: ", base::enumToStr(lir_instruction.operation)).data()
				))
			);

			const auto kind   = lirOpToOpKind(lir_instruction.operation);
			const auto output = lirOutputToOpArg(ctx, lir_instruction);

			// Add output as an argument.
			std::deque args = { output };

			// Add other arguments.
			for (auto&& lir_location: lir_instruction.arguments)
				args.push_back(lirValueToOpArg(ctx, lir_location));


			// Transforms arguments.
			if (kind == OpKind::add || kind == OpKind::sub || kind == OpKind::mul
			    || kind == OpKind::div || kind == OpKind::mod) {
				CORE_ASSERT(args.size() == 3, "Invalid arithmetic operation argument count");
				if (args[0] == args[1]) {
					// This resolves e.g. `a = a + b;` by doing `a = b`
					args.pop_front();
				} else {
					// This resolves e.g. `a = b + c;`
					// by splitting it into two instructions:
					// a = b;
					// a += c;
					pushInstruction(ctx.bytecode_func, { OpKind::mov, args[0], args[1] });

					args.pop_front();
					args.pop_front();
					args.push_front(output);
				}
			} else if (kind == OpKind::neg) {
				CORE_ASSERT(args.size() == 2, "Invalid argument count for neg");
				if (args[0] == args[1]) {
					// a = -a;
					args.pop_back();
				} else {
					// a = -b;
					// =>
					// a = b;
					// a = -a;

					// a = b;
					pushInstruction(ctx.bytecode_func, { OpKind::mov, args[0], args[1] });
					args.pop_back();
				}
			} else if (kind == OpKind::call) {
				handleAddCall(ctx, args);
				return;
			}

			InstructionBuilder instr(kind);

			for (const auto& arg: args) instr.pushArgs(arg);

			pushInstruction(ctx.bytecode_func, instr);
		}
	}

	void addTerminator(AddLirFuncContext& ctx, const lir::BlockRef lir_block) {
		const auto& terminator = lir_block->terminator;

		pushInstruction(
			ctx.bytecode_func,
			instructions::Comment(base::StrID(
				base::strConcat("Terminator: ", base::enumToStr(terminator.operation)).data()
			))
		);

		if (terminator.operation == lir::Operation::Branch) {
			auto bool_arg    = lirValueToOpArg(ctx, terminator.arguments.at(0));
			auto true_block  = lirValueToOpArg(ctx, terminator.arguments.at(1));
			auto false_block = lirValueToOpArg(ctx, terminator.arguments.at(2));

			variant_match(terminator.arguments.at(0).getVariant()) {
				variant_case(bool, value) {
					if (value)
						pushInstruction(ctx.bytecode_func, { OpKind::jmp, true_block });
					else
						pushInstruction(ctx.bytecode_func, { OpKind::jmp, false_block });
				}
				variant_default {
					pushInstruction(
						ctx.bytecode_func, { OpKind::cmpEq, bool_arg, vm::opargs::Immediate{ 1 } }
					);
					pushInstruction(ctx.bytecode_func, { OpKind::jmpIf, true_block });
					pushInstruction(ctx.bytecode_func, { OpKind::jmpIfNot, false_block });
				}
			}

		} else {
			InstructionBuilder terminator_instr;
			terminator_instr.setKind(lirTerminatorToOpKind(terminator.operation));

			// Since VM does not support `return X;` operation, we must move the value to
			// 0th index and then return.
			if (terminator.operation == lir::Operation::ReturnValue) {
				CORE_ASSERT(
					terminator.arguments.size() == 1, "Invalid number of arguments for value-return."
				);
				pushInstruction(
					ctx.bytecode_func,
					{
						OpKind::mov,
						vm::opargs::StackLocal64(base::StrID("ret_val")),
						lirValueToOpArg(ctx, terminator.arguments.at(0)),
					}
				);
			} else {
				for (auto&& lir_location: terminator.arguments)
					terminator_instr.pushArg(lirValueToOpArg(ctx, lir_location));
			}

			pushInstruction(ctx.bytecode_func, terminator_instr);
		}
	}

	Module::Module(
		query::Context&                         query_ctx,
		base::StrID                             module_id,
		const std::vector<CRef<lir::Function>>& functions,
		const std::vector<BackendDVMGlobal>&    globals
	):
		  module_id(module_id),
		  valid_program(ValidProgram::withBuiltins()) {
		CodeCollection                   compiled_types;
		CodeCollection                   global_data;
		std::vector<CRef<lir::Function>> ctors;
		std::vector<CRef<lir::Function>> dtors;

		for (const auto& lir_function: functions)
			insertTypesUsedByFunction(compiled_types.types, lir_function);

		for (const auto& global: globals) {
			auto global_type = getTypeFromLayout(*global.lir_global.layout);
			compiled_types.types.push_back(global_type);
			// @TODO: add a isConst to DVM and initial values, add source position to GlobalVariables
			global_data.global_data.push_back(GlobalData{
				{}, global.lir_global.mangled_name, typeName(global_type) });

			// @TODO: handle ctors and dtors in DMV properly
			if (global.global_ctor.has_value()) {
				insertTypesUsedByFunction(compiled_types.types, global.global_ctor.value());
				ctors.emplace_back(global.global_ctor.value());
			}
			if (global.global_dtor.has_value()) {
				insertTypesUsedByFunction(compiled_types.types, global.global_dtor.value());
				dtors.emplace_back(global.global_dtor.value());
			}
		}

		// Insert and validate types:
		valid_program.insertCode(compiled_types);

		// Insert and validate global data:
		valid_program.insertCode(global_data);

		CodeCollection compiled_functions;

		auto process_function = [&](CRef<lir::Function> lir_function) {
			std::cerr << "Adding function: " << lir_function->mangled_name.strView() << "\n";

			AddLirFuncContext ctx(query_ctx, lir_function, valid_program.types());
			initLocals(ctx);

			for (auto&& lir_block: lir_function->block_order) registerBlock(ctx, lir_block);

			for (auto&& lir_block: lir_function->block_order) {
				addBlockLabel(ctx, lir_block);
				for (auto& lir_instruction: lir_block->instructions)
					addLirInstruction(ctx, lir_instruction);

				addTerminator(ctx, lir_block);
			}

			compiled_functions.functions.emplace_back(std::move(ctx.bytecode_func));
		};

		for (const auto& ctor: ctors) process_function(ctor);

		//@TODO: add dtors when implemented

		for (const auto& lir_function: functions) process_function(lir_function);
		// Insert and validate functions:
		valid_program.insertCode(compiled_functions);
	}

	vm::code::CodeCollection Module::build() const {
		return valid_program.produceValidCodeCollection();
	}
}
