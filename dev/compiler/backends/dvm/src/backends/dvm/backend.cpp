#include "backend.hpp"
#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include "builders.hpp"
#include <vm/code_data/opcode_args.hpp>
#include "serializer.hpp"
#include <base/variant.hpp>
#include "instructions.hpp"
#include <ostream>
#include <vm/preprocessor/parser/type_of_data.hpp>
#include <typesystem/lower/type_layout.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <ranges>
#include "utils.hpp"

namespace compiler::backend_vm {
	void Module::buildRepr(std::ostream& out) const { serialize(file_builder.build(), out); }

	vm::parser::TypeOfData getTypeFromLayout(const tsl::TypeLayout& layout) {
		variant_match(layout()) {
			variant_case_novalue(tsl::EmptyTypeLayout) {
				return vm::parser::PrimitiveType{ .name = base::StrID("void"), .size = 0 };
			}
			variant_case_novalue(tsl::IntegralTypeLayout) {
				auto bits = usize(layout.getSize());
				if (bits % 8 != 0) CORE_PANIC("Integral type size not divisible by 8");
				usize       bytes = bits / 8;
				std::string name  = "i" + std::to_string(bits);

				return vm::parser::PrimitiveType{ .name = base::StrID(name.c_str()),
					                              .size = bytes };
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

				return vm::parser::PrimitiveType{ .name = base::StrID(name.c_str()),
					                              .size = bytes };
			}
			variant_default {
				CORE_PANIC(
					base::strConcat("Type not handled yet: ", layout.toStringIdentification())
				);
			}
		}
		CORE_UNREACHABLE();
	}

	namespace {
		struct AddLirFuncContext {
			CRef<lir::Function> lir_function;
			FunctionBuilder     func_builder;

			base::HashMap<usize, base::StrID>                block_id_to_label;
			base::Map<lir::LocalRef, usize>                  lir_local_to_stack;
			base::Map<lir::LocalRef, vm::parser::TypeOfData> lir_local_types;

			base::Map<lir::LocalRef, u64> variable_to_id;
			base::Map<lir::BlockRef, u64> block_to_id;

			AddLirFuncContext(
				CRef<lir::Function>                                       lir_function,
				const base::HashMap<base::StrID, vm::parser::TypeOfData>& available_types
			):
				  lir_function(lir_function),
				  func_builder(lir_function->name, available_types) {
				variable_to_id = lir_function->getLocalVariableIDs();
				block_to_id    = lir_function->getBlockIDs();

				for (auto&& var: lir_function->local_list) {
					// This is most likely redundant
					CORE_ASSERT(!lir_local_to_stack.contains(var.ref()), "Duplicated lir local");

					auto vm_type = getTypeFromLayout(var->layout);
					lir_local_types.put(var.ref(), vm_type);

					auto tp_name = VISIT(vm_type, tp, return tp.name);
					auto offset  = func_builder.initType(tp_name);
					lir_local_to_stack.put(var.ref(), offset);
				}
			}
		};

		void insertTypes(Ref<CodeFileBuilder> file_builder, CRef<lir::Function> lir_function) {
			auto&& local_layouts
				= lir_function->local_list
			    | std::views::transform([](auto&& local) { return local->layout; });

			for (auto&& layout: local_layouts) file_builder->addType(getTypeFromLayout(layout));
		}

		void addBlockLabel(AddLirFuncContext& ctx, lir::BlockRef block) {
			auto id = ctx.block_to_id.atMaybe(block).expect("id of block not found");

			auto label_name       = base::strConcat(ctx.lir_function->name, "_label_", id);
			auto [block_entry, _] = ctx.block_id_to_label.put(id, base::StrID(label_name.data()));
			ctx.func_builder.addInstruction(Op_label{ block_entry->second });
		}

		vm::opargs::OpCodeArg
			lirOutputToOpArg(AddLirFuncContext& ctx, const lir::Instruction& lir_instruction) {
			const lir::LocalRef output   = lir_instruction.output.value();
			auto&&              var_type = ctx.lir_local_types[output];

			variant_match(var_type) {
				variant_case(vm::parser::PrimitiveType, primitive) {
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

				variant_case(vm::parser::PointerType, pointer) {
					return vm::opargs::StackLocalPtr(i64(ctx.lir_local_to_stack[output]));
				}
				NOIMPL_CASE(vm::parser::StaticTableType, "add_instr")
				NOIMPL_CASE(vm::parser::DynamicTableType, "add_instr")
				NOIMPL_CASE(vm::parser::DataType, "add_instr")
				NOIMPL_CASE(vm::parser::VariantType, "add_instr")
				variant_case(vm::parser::FunctionType, function_tp) {
					return vm::opargs::FunctionName(function_tp.name);
				}
			}
			CORE_UNREACHABLE();
		}

		vm::opargs::OpCodeArg
			lirArgToOpArg(AddLirFuncContext& ctx, const lir::LirLocation& lir_location) {
			variant_match(lir_location.getVariant()) {
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

		OpKind lirOpToOpKind(lir::Operation operation) {
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

		OpKind lirTerminatorToOpKind(lir::Operation terminator) {
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
	}

	void Module::addLirFunction(CRef<lir::Function> lir_function) {
		std::cerr << "Adding function: " << lir_function->name.strView() << "\n";

		insertTypes(&file_builder, lir_function);

		AddLirFuncContext ctx(lir_function, file_builder.getAvailableTypes());

		for (auto&& lir_block: lir_function->blocks) {
			for (auto& lir_instruction: lir_block->instructions) {
				addBlockLabel(ctx, lir_block.ref());

				// Insert a comment about operation type.
				// @TODO: Improve this to contain more information.
				ctx.func_builder.addInstruction(Comment{ base::StrID(
					base::strConcat("Operation: ", base::enumToStr(lir_instruction.operation))
						.data()
				) });

				InstructionBuilder instr;
				instr.setKind(lirOpToOpKind(lir_instruction.operation));

				// Add output as an argument.
				instr.pushArg(lirOutputToOpArg(ctx, lir_instruction));

				// Add other arguments.
				for (auto&& lir_location: lir_instruction.arguments)
					instr.pushArg(lirArgToOpArg(ctx, lir_location));
				ctx.func_builder.addInstruction(instr);
			}

			// @TODO
			ctx.func_builder.addInstruction(Comment{ base::StrID(
				base::strConcat("Terminator: ", base::enumToStr(lir_block->terminator.operation))
					.data()
			) });

			InstructionBuilder terminator_instr;
			terminator_instr.setKind(lirTerminatorToOpKind(lir_block->terminator.operation));

			for (auto&& lir_location: lir_block->terminator.arguments)
				terminator_instr.pushArg(lirArgToOpArg(ctx, lir_location));

			ctx.func_builder.addInstruction(terminator_instr);
		}

		file_builder.addFunction(ctx.func_builder);
	}

	Module::Module(base::StrID module_id): module_id(module_id) {}
}
