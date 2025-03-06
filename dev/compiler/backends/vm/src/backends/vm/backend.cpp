#include "backend.hpp"
#include "base/exceptions.hpp"
#include "base/string_id.hpp"
#include "builders.hpp"
#include "code_data/opcode_args.hpp"
#include "serializer.hpp"
#include <base/variant.hpp>
#include "instructions.hpp"
#include <ostream>
#include <preprocessor/parser/types_of_data.hpp>
#include <typesystem/lower/type_layout.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <ranges>

namespace compiler::backend_vm {
	void Module::buildRepr(std::ostream& out) const { serialize(file_builder.build(), out); }

	vm::TypeOfData getTypeFromLayout(const tsl::TypeLayout& layout) {
		variant_match(layout()) {
			variant_case_novalue(tsl::EmptyTypeLayout) {
				return vm::PrimitiveType{ .name = base::StrID("void"), .size = 0 };
			}
			variant_case_novalue(tsl::IntegralTypeLayout) {
				auto bits = usize(layout.getSize());
				if (bits % 8 != 0) CORE_PANIC("Integral type size not divisible by 8");
				usize       bytes = bits / 8;
				std::string name  = "i" + std::to_string(bits);

				return vm::PrimitiveType{ .name = base::StrID(name.c_str()), .size = bytes };
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

				return vm::PrimitiveType{ .name = base::StrID(name.c_str()), .size = bytes };
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
		void insertTypes(Ref<CodeFileBuilder> file_builder, CRef<lir::Function> lir_function) {
			auto&& local_layouts
				= lir_function->local_list
			    | std::views::transform([](auto&& local) { return local->layout; });

			for (auto&& layout: local_layouts) file_builder->addType(getTypeFromLayout(layout));
		}
	}

	void Module::addLirFunction(CRef<lir::Function> lir_function) {
		std::cerr << "Adding function: " << lir_function->name.strView() << "\n";

		insertTypes(&file_builder, lir_function);

		FunctionBuilder function(lir_function->name, file_builder.getAvailableTypes());

		auto                              variable_to_id = lir_function->getLocalVariableIDs();
		auto                              block_to_id    = lir_function->getBlockIDs();
		base::HashMap<usize, base::StrID> block_id_to_label;

		base::Map<lir::LocalRef, usize>          lir_local_to_stack;
		base::Map<lir::LocalRef, vm::TypeOfData> lir_local_types;

		for (auto&& var: lir_function->local_list) {
			// This is most likely redundant
			CORE_ASSERT(!lir_local_to_stack.contains(var.ref()), "Duplicated lir local");

			auto vm_type = getTypeFromLayout(var->layout);
			lir_local_types.put(var.ref(), vm_type);

			auto tp_name = VISIT(vm_type, tp, return tp.name);
			auto offset  = function.initType(tp_name);
			lir_local_to_stack.put(var.ref(), offset);
		}

		for (auto&& lir_block: lir_function->blocks) {
			// block->terminator
			auto id = block_to_id.atMaybe(lir_block.ref()).expect("id of block not found");

			auto label_name       = base::strConcat(lir_function->name, "_label_", id);
			auto [block_entry, _] = block_id_to_label.put(id, base::StrID(label_name.data()));
			function.addInstruction(Op_label{ block_entry->second });

			for (auto& lir_instruction: lir_block->instructions) {
				// @TODO
				function.addInstruction(Comment{ base::StrID(
					base::strConcat("Operation: ", base::enumToStr(lir_instruction.operation))
						.data()
				) });

				InstructionBuilder instr;

				switch (lir_instruction.operation) {
				case lir::Operation::Assign:
					instr.setKind(OpKind::mov);
					break;
				case lir::Operation::IntegerAdd:
					instr.setKind(OpKind::add);
					break;
				case lir::Operation::IntegerSub:
					instr.setKind(OpKind::sub);
					break;
				case lir::Operation::IntegerMul:
					instr.setKind(OpKind::mul);
					break;
				case lir::Operation::IntegerUDiv:
					instr.setKind(OpKind::div);
					break;
				case lir::Operation::IntegerSDiv:
					instr.setKind(OpKind::div);
					break;
				case lir::Operation::IntegerUMod:
					instr.setKind(OpKind::mod);
					break;
				case lir::Operation::IntegerSMod:
					instr.setKind(OpKind::mod);
					break;
				case lir::Operation::IntegerNeg:
					instr.setKind(OpKind::neg);
					break;
				case lir::Operation::IntegerULt:
				case lir::Operation::IntegerSLt:
				default:
					std::cerr << base::strConcat(
						"Invalid operation: ", base::enumToStr(lir_instruction.operation)
					) << '\n';
					// default:
					// 	CORE_PANIC("Invalid operation: ",
					// base::enumToStr(lir_instruction.operation));
				}


				// Add output as an argument.
				const lir::LocalRef output   = lir_instruction.output.value();
				auto&&              var_type = lir_local_types[output];

				variant_match(var_type) {
					variant_case(vm::PrimitiveType, primitive) {
						if (primitive.size != 8 && primitive.size != 4)
							throw base::NotYetImplemented(base::strConcat(
								"Primitives of sizes different than 64 | 32 bits are not "
								"supported YET, name: ",
								primitive.name,
								", size: ",
								primitive.size
							));
						if (primitive.size == 8)
							instr.pushArg(vm::opargs::StackLocalI64{
								i64(lir_local_to_stack[output]) });
						if (primitive.size == 4)
							instr.pushArg(vm::opargs::StackLocalI32{
								i64(lir_local_to_stack[output]) });
					}

					variant_case(vm::PointerType, pointer) {
						instr.pushArg(vm::opargs::StackLocalPtr(i64(lir_local_to_stack[output])));
					}
					NOIMPL_CASE(vm::StaticTableType, "add_instr")
					NOIMPL_CASE(vm::DynamicTableType, "add_instr")
					NOIMPL_CASE(vm::DataType, "add_instr")
					NOIMPL_CASE(vm::VariantType, "add_instr")
					variant_case(vm::FunctionType, function_tp) {
						instr.pushArg(vm::opargs::FunctionName(function_tp.name));
					}
				}

				// Add other arguments.
				for (auto&& arg: lir_instruction.arguments) {
					variant_match(arg.getVariant()) {
						variant_case(i64, value) instr.pushArg(vm::opargs::Immediate{ value });
						variant_case(bool, value) instr.pushArg(vm::opargs::Immediate{ value });
						variant_case(lir::LocalRef, local_ref) instr.pushArg(
							vm::opargs::StackLocalI64{ i64(lir_local_to_stack[local_ref]) }
						);
						variant_case(lir::BlockRef, block_ref) {
							instr.pushArg(vm::opargs::Label{ base::StrID(
								base::strConcat(lir_function->name, "_label_", id).data()
							) });
						}
					}
				}
				function.addInstruction(instr.build());
			}

			// @TODO
			function.addInstruction(Comment{ base::StrID(
				base::strConcat("Terminator: ", base::enumToStr(lir_block->terminator.operation))
					.data()
			) });

			switch (lir_block->terminator.operation) {
			case lir::Operation::ReturnVoid: {
			}
			case lir::Operation::ReturnValue:
			case lir::Operation::Jump:
			case lir::Operation::Branch:

			case lir::Operation::COUNT:
			case lir::Operation::Uninitialized:
			default:
				std::cerr << base::strConcat(
					"Invalid terminator: ", base::enumToStr(lir_block->terminator.operation)
				) << '\n';
				// default:
				// 	CORE_PANIC(
				// 		"Invalid terminator: ", base::enumToStr(lir_block->terminator.operation)
				// 	);
			}
		}

		file_builder.addFunction(function);
	}

	Module::Module(base::StrID module_id): module_id(module_id) {}
}
