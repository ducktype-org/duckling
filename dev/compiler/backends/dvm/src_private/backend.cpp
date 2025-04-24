#include <backends/dvm/backend.hpp>
#include <helios/symbols/simple.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <algorithm>
#include <ostream>
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

namespace compiler::backend_vm {
	namespace {
		vm::code::TypeOfData getTypeFromLayout(const tsl::TypeLayout& layout) {
			variant_match(layout()) {
				variant_case_novalue(tsl::EmptyTypeLayout) {
					return vm::code::PrimitiveType(base::StrID("void"), 0);
				}
				variant_case_novalue(tsl::IntegralTypeLayout) {
					auto bits = usize(layout.getSize());
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

		struct AddLirFuncContext {
			CRef<lir::Function> lir_function;
			FunctionBuilder     func_builder;

			base::HashMap<usize, base::StrID>    block_id_to_label;
			base::Map<lir::LocalRef, usize>      lir_local_to_stack;
			base::Map<lir::LocalRef, TypeOfData> lir_local_types;

			base::Map<lir::LocalRef, u64>                variable_to_id;
			base::Map<lir::BlockRef, u64>                block_to_id;
			const vm::TypeMetadata&                      types;
			const base::HashMap<base::StrID, TypeOfData> TYPE_OF_DATA;

			AddLirFuncContext(CRef<lir::Function> lir_function, const TypeContext& type_context):
				  lir_function(lir_function),
				  func_builder(lir_function->name, {}, type_context),
				  types(type_context.getMetadata()),
				  TYPE_OF_DATA([&type_context] {
					  base::HashMap<base::StrID, TypeOfData> map;
					  for (auto&& type: type_context.getTypes())
						  map.put(VISIT(type, tp, return tp.name), type);
					  return map;
				  }()) {
				variable_to_id = lir_function->getLocalVariableIDs();
				block_to_id    = lir_function->getBlockIDs();
			}
		};

		/**
		 * @brief Generate `init_type` instruction and return the offset on the stack of the variable.
		 */
		usize initType(AddLirFuncContext& ctx, base::StrID type_name) {
			auto offset = ctx.func_builder.initType(instructions::Op_init_type(type_name));
			ctx.func_builder.addInstruction(
				instructions::Comment(base::StrID(base::strConcat("Variable\t\t\t", offset).data()))
			);
			return offset;
		}

		void initLocals(AddLirFuncContext& ctx) {
			auto func_type = ctx.types.at(ctx.lir_function->name);
			CORE_ASSERT(
				std::holds_alternative<FunctionType>(ctx.TYPE_OF_DATA.at(ctx.lir_function->name)),
				"Type not functional"
			);
			auto func_type_tod
				= std::get<FunctionType>(ctx.TYPE_OF_DATA.at(ctx.lir_function->name));
			base::HashMap<usize, usize> param_offsets;
			const auto                  param_count = *func_type->getParameterCount();

			usize prev_param_offset = func_type->getResultType().value()->getSize(
			);  // at 0th index is the result storage
			for (usize idx = 0; idx < param_count; idx++) {
				param_offsets.put(idx, prev_param_offset);
				prev_param_offset += func_type->getNthParameterType(idx).value()->getSize();
			}

			// Save locals offset
			for (const auto& var: ctx.lir_function->local_list) {
				// This is most likely redundant
				CORE_ASSERT(!ctx.lir_local_to_stack.contains(var.ref()), "Duplicated lir local");

				auto vm_type = getTypeFromLayout(var->layout);
				ctx.lir_local_types.put(var.ref(), vm_type);
				match_optional(var->parameter_index) {
					opt_some(param_idx) {
						// In this case we are handling a parameter
						CORE_ASSERT(
							vm_type == ctx.TYPE_OF_DATA.at(func_type_tod.parameters.at(param_idx)),
							"getTypeFromLayout created an invalid type..."
						);

						ctx.lir_local_to_stack.put(var.ref(), param_offsets[param_idx]);
					}
					opt_none {
						// In this case we are handling a regular variable
						auto tp_name = VISIT(vm_type, tp, return tp.name);
						auto offset  = initType(ctx, tp_name);
						ctx.lir_local_to_stack.put(var.ref(), offset);
					}
				}
			}
		}

		void insertFunctionType(TypeContextBuilder& type_context, CRef<lir::Function> lir_function) {
			auto param_types
				= lir_function->parameter_layouts
			    | std::views::transform([](auto&& layout) { return getTypeFromLayout(layout); });

			std::ranges::for_each(param_types, [&](auto&& type) { type_context.addType(type); });

			auto param_names
				= param_types
			    | std::views::transform([](auto&& type) { return VISIT(type, tp, return tp.name); })
			    | std::ranges::to<std::vector>();

			auto result_type = getTypeFromLayout(lir_function->return_type_layout);
			type_context.addType(result_type);

			auto result_type_name = VISIT(result_type, type, return type.name);

			if (lir_function->name != "main") {
				type_context.addType(FunctionType{
					lir_function->name, param_names, result_type_name });
			}
		}

		void insertTypes(TypeContextBuilder& type_context, CRef<lir::Function> lir_function) {
			// Insert function type
			insertFunctionType(type_context, lir_function);

			// Insert local types
			auto local_layouts = lir_function->local_list
			                   | std::views::transform([](auto&& local) { return local->layout; });

			for (const auto& layout: local_layouts) type_context.addType(getTypeFromLayout(layout));
		}

		void registerBlock(AddLirFuncContext& ctx, lir::BlockRef block) {
			auto id         = ctx.block_to_id.at(block);
			auto label_name = base::strConcat("label_", id);
			ctx.block_id_to_label.put(id, base::StrID(label_name.data()));
		}

		void addBlockLabel(AddLirFuncContext& ctx, lir::BlockRef block) {
			auto id = ctx.block_to_id.at(block);
			ctx.func_builder.addInstruction(instructions::Op_label{ ctx.block_id_to_label[id] });
		}

		vm::opargs::OpCodeArg outputToOpArg(vm::code::TypeOfData type, const i64 offset) {
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
					if (primitive.size == 8) return vm::opargs::StackLocalI64{ offset };
					if (primitive.size == 4) return vm::opargs::StackLocalI32{ offset };
					if (primitive.size == 2) return vm::opargs::StackLocalI16{ offset };
					if (primitive.size == 1) return vm::opargs::StackLocalI8{ offset };
				}
				variant_case(vm::code::PointerType, pointer) {
					return vm::opargs::StackLocalPtr(offset);
				}

				INVALID_CASE(vm::code::StaticTableType, "output target");
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
			const lir::LocalRef output   = lir_instruction.output.value();
			auto&&              var_type = ctx.lir_local_types[output];
			return outputToOpArg(var_type, i64(ctx.lir_local_to_stack[output]));
		}

		constexpr vm::opargs::OpCodeArg lirValueToOpArg(
			AddLirFuncContext& ctx, const lir::LIRValue& lir_value
		) {
			variant_match(lir_value.getVariant()) {
				variant_case(i64, value) return vm::opargs::Immediate{ value };
				variant_case(bool, value) return vm::opargs::Immediate{ value };
				variant_case(
					lir::LocalRef, local_ref
				) return vm::opargs::StackLocalI64{ i64(ctx.lir_local_to_stack[local_ref]) };
				variant_case(lir::BlockRef, block_ref) {
					return vm::opargs::Label{ ctx.block_id_to_label[ctx.block_to_id[block_ref]] };
				}
				variant_case(lir::FunctionLiteral, function) {
					return vm::opargs::FunctionName(helios::name(function.helios_id));
				}
				variant_default { CORE_PANIC("Unhandled value case"); }
			}
			CORE_UNREACHABLE();
		}

		constexpr vm::opargs::OpCodeArg modifyOffsetOpArg(
			const vm::opargs::OpCodeArg& op_arg, const i64 new_offset
		) {
			variant_match(op_arg) {
#define OFFSET_CASE(type)                         \
	variant_case(vm::opargs::type, offset_type) { \
		auto copy   = offset_type;                \
		copy.offset = new_offset;                 \
		return copy;                              \
	}
				FOR_EACH(OFFSET_CASE, VM_OPARG_OFFSET_TYPES);
#undef OFFSET_CASE
				variant_default { CORE_PANIC("Given op_arg is not an offset kind"); }
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
			auto called_func = ctx.types.at(called_func_name);

			// Init result type

			auto func_result_storage_offset
				= initType(ctx, called_func->getResultType().value()->getName());
			auto func_result_argument = modifyOffsetOpArg(
				lir_result_argument, base::safeIntConv<i64>(func_result_storage_offset)
			);

			// Instantiate function parameters on the stack.

			auto func_params = called_func->getParameters().value();
			for (const auto& [op_arg, param_type]: std::views::zip(args, func_params)) {
				auto        type_of_argument = param_type;
				base::StrID type_name        = type_of_argument->getName();
				std::cerr << "Initializing: " << type_name.str() << '\n';
				auto offset = initType(ctx, type_name);

				InstructionBuilder mov_arg(OpKind::mov);
				mov_arg.pushArg(outputToOpArg(ctx.TYPE_OF_DATA[type_name], i64(offset)));
				mov_arg.pushArg(op_arg);
				ctx.func_builder.addInstruction(mov_arg);
			}

			ctx.func_builder.addInstruction({ OpKind::call, called_func_arg });

			ctx.func_builder.addInstruction({
				OpKind::mov,
				lir_result_argument,
				func_result_argument,
			});
			ctx.func_builder.addInstruction(instructions::Op_deinit());  // Deinit func result
		}

		void addLirInstruction(AddLirFuncContext& ctx, const lir::Instruction& lir_instruction) {
			// Insert a comment about operation type.
			// @TODO: Improve this to contain more information.
			ctx.func_builder.addInstruction(vm::code::instructions::Comment(base::StrID(
				base::strConcat("Operation: ", base::enumToStr(lir_instruction.operation)).data()
			)));

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
					ctx.func_builder.addInstruction({ OpKind::mov, args[0], args[1] });

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
					ctx.func_builder.addInstruction({ OpKind::mov, args[0], args[1] });
					args.pop_back();
				}
			} else if (kind == OpKind::call) {
				handleAddCall(ctx, args);
				return;
			}

			InstructionBuilder instr(kind);

			for (const auto& arg: args) instr.pushArgs(arg);

			ctx.func_builder.addInstruction(instr);
		}
	}

	void addTerminator(AddLirFuncContext& ctx, const lir::BlockRef lir_block) {
		const auto& terminator = lir_block->terminator;

		ctx.func_builder.addInstruction(instructions::Comment(base::StrID(
			base::strConcat("Terminator: ", base::enumToStr(terminator.operation)).data()
		)));

		if (terminator.operation == lir::Operation::Branch) {
			auto bool_arg    = lirValueToOpArg(ctx, terminator.arguments.at(0));
			auto true_block  = lirValueToOpArg(ctx, terminator.arguments.at(1));
			auto false_block = lirValueToOpArg(ctx, terminator.arguments.at(2));

			variant_match(terminator.arguments.at(0).getVariant()) {
				variant_case(bool, value) {
					if (value)
						ctx.func_builder.addInstruction({ OpKind::jmp, true_block });
					else
						ctx.func_builder.addInstruction({ OpKind::jmp, false_block });
				}
				variant_default {
					ctx.func_builder.addInstruction(
						{ OpKind::cmpEq, bool_arg, vm::opargs::Immediate{ 1 } }
					);
					ctx.func_builder.addInstruction({ OpKind::jmpIf, true_block });
					ctx.func_builder.addInstruction({ OpKind::jmpIfNot, false_block });
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
				ctx.func_builder.addInstruction({
					OpKind::mov,
					vm::opargs::StackLocalI64(0),
					lirValueToOpArg(ctx, terminator.arguments.at(0)),
				});
			} else {
				for (auto&& lir_location: terminator.arguments)
					terminator_instr.pushArg(lirValueToOpArg(ctx, lir_location));
			}

			ctx.func_builder.addInstruction(terminator_instr);
		}
	}

	Module::Module(base::StrID module_id, const std::vector<CRef<lir::Function>>& functions):
		  module_id(module_id),
		  type_context_builder(vm::code::getBuiltinTypes()) {
		for (const auto& lir_function: functions) insertTypes(type_context_builder, lir_function);

		TypeContext type_context = type_context_builder.build();
		code.types               = type_context.getTypes();

		for (const auto& lir_function: functions) {
			std::cerr << "Adding function: " << lir_function->name.strView() << "\n";

			AddLirFuncContext ctx(lir_function, type_context);
			initLocals(ctx);

			for (auto&& lir_block: lir_function->block_order) registerBlock(ctx, lir_block);

			for (auto&& lir_block: lir_function->block_order) {
				addBlockLabel(ctx, lir_block);
				for (auto& lir_instruction: lir_block->instructions)
					addLirInstruction(ctx, lir_instruction);

				addTerminator(ctx, lir_block);
			}


			code.functions.emplace_back(ctx.func_builder.build());
		}
	}

	vm::code::CodeCollection Module::build() const { return code; }
}
