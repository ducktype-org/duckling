#include "backend.hpp"
#include "base/string_id.hpp"
#include "builders.hpp"
#include "query_framework/utils/with_context_do.hpp"
#include "serializer.hpp"
#include <algorithm>
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

		base::HashMap<lir::LocalRef, usize>          lir_local_to_stack;
		base::HashMap<lir::LocalRef, vm::TypeOfData> lir_local_types;

		for (auto&& var: lir_function->local_list) {
			// This is most likely redundant
			CORE_ASSERT(!lir_local_to_stack.contains(var.ref()), "Duplicated lir local");

			auto vm_type = getTypeFromLayout(var->layout);
			lir_local_types.put(var.ref(), vm_type);

			auto tp_name   = VISIT(vm_type, tp, return tp.name);
			auto offset = function.initType(tp_name);
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
				// std::string args;
				// for(auto&& arg: lir_instruction.arguments)
				// args += arg.
				// block.addInstruction(Comment {lir_instruction.arguments})

				switch (lir_instruction.operation) {
				case lir::Operation::Assign: {
					const lir::LocalRef output = lir_instruction.output.value();
					const auto          arg0   = lir_instruction.arguments.at(0);
					break;
				}
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
					std::cerr << base::strConcat(
						"Invalid operation: ", base::enumToStr(lir_instruction.operation)
					) << '\n';
					// default:
					// 	CORE_PANIC("Invalid operation: ",
					// base::enumToStr(lir_instruction.operation));
				}
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
