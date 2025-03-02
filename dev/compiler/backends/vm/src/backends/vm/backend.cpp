#include "backend.hpp"
#include "base/variant.hpp"
#include "instructions.hpp"
#include <preprocessor/parser/types_of_data.hpp>
#include <typesystem/lower/type_layout.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <ranges>

namespace compiler::backend_vm {

	void Module::buildRepr(std::ostream& out) const { file_builder.build().serialize(out); }

	vm::TypeOfData Module::getTypeFromLayout(const tsl::TypeLayout& layout) {
		variant_match(layout()) {
			variant_case_novalue(tsl::EmptyTypeLayout) {
				return vm::PrimitiveType{ .name=base::StrID("void"), .size=0 };
			}
			variant_case_novalue(tsl::IntegralTypeLayout) {
				auto bits = usize(layout.getSize());
				if (bits % 8 != 0) {
					CORE_PANIC("Integral type size not divisible by 8");
				}
				usize bytes = bits / 8;
				std::string name = "i" + std::to_string(bits);

				return vm::PrimitiveType{
					.name=base::StrID(name.c_str()), .size=bytes
				};
			}
			variant_default {
				CORE_PANIC(
					base::strConcat("Type not handled yet: ", layout.toStringIdentification())
				);
			}
		}
		CORE_UNREACHABLE();
	}

	void Module::addLirFunction(CRef<lir::Function> lir_function) {
		std::cerr << "Adding function: " << lir_function->name.strView() << "\n";
		auto&& local_layouts = lir_function->local_list
		                     | std::views::transform([](auto&& local) { return local->layout; });
		for (auto&& layout: local_layouts) {
			// @TODO
			// Transform layout to type:
			// file_builder.addType(const vm::TypeOfData &type)
		}

		auto block_to_id = lir_function->getBlockIDs();
		for (auto&& lir_block: lir_function->blocks) {
			// block->terminator
			auto         id = block_to_id.atMaybe(lir_block.ref()).expect("id of block not found");
			BlockBuilder block;
			std::string  label_str = base::strConcat(lir_function->name, "_label_", id);
			block.addInstruction(Op_label{ base::StrID(label_str.data()) });

			for (auto& instruction: lir_block->instructions) {
				// @TODO
				switch (instruction.operation) {
				case lir::Operation::Assign:
				case lir::Operation::IntegerAdd:
				case lir::Operation::IntegerSub:
				case lir::Operation::IntegerMul:
				case lir::Operation::IntegerUDiv:
				case lir::Operation::IntegerSDiv:
				case lir::Operation::IntegerUMod:
				case lir::Operation::IntegerSMod:
				case lir::Operation::IntegerULt:
				case lir::Operation::IntegerSLt:
				case lir::Operation::IntegerNeg:
				default:
					CORE_PANIC("Invalid operation: ", base::enumToStr(instruction.operation));
				}
			}

			// @TODO
			switch (lir_block->terminator.operation) {
			case lir::Operation::ReturnVoid:
			case lir::Operation::ReturnValue:
			case lir::Operation::Jump:
			case lir::Operation::Branch:

			case lir::Operation::COUNT:
			case lir::Operation::Uninitialized:
			default:
				CORE_PANIC("Invalid terminator: ", base::enumToStr(lir_block->terminator.operation));
			}
		}
	}

	Module::Module(base::StrID module_id): module_id(module_id) {}
}
