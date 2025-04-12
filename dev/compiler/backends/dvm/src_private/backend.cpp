#include <backends/dvm/backend.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include "vm/bytecode/builders/builders.hpp"
#include "vm/bytecode/instructions.hpp"
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <ostream>
#include <ranges>

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

			base::Map<lir::LocalRef, u64> variable_to_id;
			base::Map<lir::BlockRef, u64> block_to_id;

			AddLirFuncContext(CRef<lir::Function> lir_function, const TypeContext& types):
				  lir_function(lir_function),
				  func_builder(lir_function->name, types) {
				variable_to_id = lir_function->getLocalVariableIDs();
				block_to_id    = lir_function->getBlockIDs();

				// init all variables
				for (auto&& var: lir_function->local_list) {
					// This is most likely redundant
					CORE_ASSERT(!lir_local_to_stack.contains(var.ref()), "Duplicated lir local");

					auto vm_type = getTypeFromLayout(var->layout);
					lir_local_types.put(var.ref(), vm_type);

					auto tp_name = VISIT(vm_type, tp, return tp.name);
					auto offset  = func_builder.initType(instructions::Op_init_type(tp_name));
					lir_local_to_stack.put(var.ref(), offset);
				}
			}
		};

		void
			insertFunctionType(TypeContextBuilder& type_context, CRef<lir::Function> lir_function) {
			auto param_layouts = lir_function->local_list
			                   | std::views::transform([](auto&& local) { return local->layout; });

			auto param_types = param_layouts | std::views::transform([](auto&& layout) {
								   return getTypeFromLayout(layout);
							   });

			std::ranges::for_each(param_types, [&](auto&& type) { type_context.addType(type); });

			auto param_names
				= param_types
			    | std::views::transform([](auto&& type) { return VISIT(type, tp, return tp.name); })
			    | std::ranges::to<std::vector>();

			auto result_type = getTypeFromLayout(lir_function->return_type_layout);
			type_context.addType(result_type);

			auto result_type_name = VISIT(result_type, type, return type.name);

			// @TODO: This is temporary. Look #692
			// https://github.com/ducktype-org/duckling/issues/692
			if (lir_function->name == "main") {
				type_context.addType(PrimitiveType{ base::StrID("int64"), 8 });
				type_context.addType(FunctionType{
					lir_function->name,
					{},
					base::StrID("int64"),
				});
			} else {
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

		void addBlockLabel(AddLirFuncContext& ctx, lir::BlockRef block) {
			auto id = ctx.block_to_id.atMaybe(block).expect("id of block not found");

			auto label_name       = base::strConcat(ctx.lir_function->name, "_label_", id);
			auto [block_entry, _] = ctx.block_id_to_label.put(id, base::StrID(label_name.data()));
			ctx.func_builder.addInstruction(instructions::Op_label{ block_entry->second });
		}

		vm::opargs::OpCodeArg
			lirOutputToOpArg(AddLirFuncContext& ctx, const lir::Instruction& lir_instruction) {
			const lir::LocalRef output   = lir_instruction.output.value();
			auto&&              var_type = ctx.lir_local_types[output];

			variant_match(var_type) {
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
					if (primitive.size == 8)
						return vm::opargs::StackLocalI64{ i64(ctx.lir_local_to_stack[output]) };
					if (primitive.size == 4)
						return vm::opargs::StackLocalI32{ i64(ctx.lir_local_to_stack[output]) };
					if (primitive.size == 2)
						return vm::opargs::StackLocalI16{ i64(ctx.lir_local_to_stack[output]) };
					if (primitive.size == 1)
						return vm::opargs::StackLocalI8{ i64(ctx.lir_local_to_stack[output]) };
				}

				variant_case(vm::code::PointerType, pointer) {
					return vm::opargs::StackLocalPtr(i64(ctx.lir_local_to_stack[output]));
				}
				NOIMPL_CASE(vm::code::StaticTableType, __func__)
				NOIMPL_CASE(vm::code::DynamicTableType, __func__)
				NOIMPL_CASE(vm::code::DataType, __func__)
				NOIMPL_CASE(vm::code::VariantType, __func__)
				variant_case(vm::code::FunctionType, function_tp) {
					return vm::opargs::FunctionName(function_tp.name);
				}
			}
			CORE_UNREACHABLE();
		}

		vm::opargs::OpCodeArg
			lirValueToOpArg(AddLirFuncContext& ctx, const lir::LIRValue& lir_value) {
			variant_match(lir_value.getVariant()) {
				variant_case(i64, value) return vm::opargs::Immediate{ value };
				variant_case(bool, value) return vm::opargs::Immediate{ value };
				variant_case(lir::LocalRef, local_ref) return vm::opargs::StackLocalI64{
					i64(ctx.lir_local_to_stack[local_ref])
				};
				variant_case(lir::BlockRef, block_ref) {
					return vm::opargs::Label{ ctx.block_id_to_label[ctx.block_to_id[block_ref]] };
				}
			}
			CORE_UNREACHABLE();
		}

		vm::code::builders::OpKind lirOpToOpKind(lir::Operation operation) {
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
			case lir::Operation::IntegerULt:
				throw base::NotYetImplemented(base::enumToStr(operation).str());
			case lir::Operation::IntegerSLt:
				throw base::NotYetImplemented(base::enumToStr(operation).str());
			default:
				CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
			}
			CORE_UNREACHABLE();
		}

		vm::code::builders::OpKind lirTerminatorToOpKind(lir::Operation terminator) {
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
					InstructionBuilder instr_mov;

					instr_mov.setKind(OpKind::mov);
					instr_mov.pushArg(args[0]);
					instr_mov.pushArg(args[1]);
					ctx.func_builder.addInstruction(instr_mov);

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
					InstructionBuilder instr_mov(OpKind::mov);
					instr_mov.pushArg(args[0]);
					instr_mov.pushArg(args[1]);

					auto built = instr_mov.build();
					ctx.func_builder.addInstruction(instr_mov);

					args.pop_back();
				}
			}

			vm::code::builders::InstructionBuilder instr(kind);

			for (const auto& arg: args) instr.pushArgs(arg);

			ctx.func_builder.addInstruction(instr);
		}
	}

	Module::Module(base::StrID module_id, const std::vector<CRef<lir::Function>>& functions):
		  module_id(module_id) {
		for (const auto& lir_function: functions) insertTypes(type_context_builder, lir_function);

		TypeContext type_context = type_context_builder.build();
		code.types        = type_context.getTypes();

		for (const auto& lir_function: functions) {
			std::cerr << "Adding function: " << lir_function->name.strView() << "\n";

			AddLirFuncContext ctx(lir_function, type_context);

			for (auto&& lir_block: lir_function->blocks) {
				addBlockLabel(ctx, lir_block.ref());
				for (auto& lir_instruction: lir_block->instructions)
					addLirInstruction(ctx, lir_instruction);

				ctx.func_builder.addInstruction(instructions::Comment(
					base::StrID(base::strConcat(
									"Terminator: ", base::enumToStr(lir_block->terminator.operation)
					)
				                    .data())
				));

				InstructionBuilder terminator_instr;
				terminator_instr.setKind(lirTerminatorToOpKind(lir_block->terminator.operation));

				// Since VM does not support `return X;` operation, we must move the value to 0th
				// index and then return.
				if (lir_block->terminator.operation == lir::Operation::ReturnValue) {
					InstructionBuilder move_ret(OpKind::mov);
					CORE_ASSERT(
						lir_block->terminator.arguments.size() == 1,
						"Invalid number of arguments for value-return."
					);
					move_ret.pushArgs(
						vm::opargs::StackLocalI64(0),
						lirValueToOpArg(ctx, lir_block->terminator.arguments.at(0))
					);
					ctx.func_builder.addInstruction(move_ret);
				} else {
					for (auto&& lir_location: lir_block->terminator.arguments)
						terminator_instr.pushArg(lirValueToOpArg(ctx, lir_location));
				}

				ctx.func_builder.addInstruction(terminator_instr);
				// @TODO: This will be removed by #539
				// https://github.com/ducktype-org/duckling/issues/539
				ctx.func_builder.setRetSize(
					type_context.getMetadata().at(lir_function->name)->getResultType().value()->getSize()
				);
			}

			code.functions.emplace_back(ctx.func_builder.build());
		}
	}

	vm::code::CodeCollection Module::build() const { return code; }
}
