#include "preprocessor.hpp"
#include <unordered_map>
#include <variant>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <diagnostic/logger.hpp>
#include "base/exceptions.hpp"
#include "base/maps.hpp"
#include "base/optional.hpp"
#include "base/string_id.hpp"
#include "base/variant.hpp"
#include "diagnostic/source_position.hpp"
#include "parser/parser.hpp"
#include <vm/program/opcode_args.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <expected>
#include <vector>
#include "parser/elements.hpp"
#include "parser/errors.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include "vm/program/builders/builders.hpp"
#include "vm/program/instructions.hpp"
#include "vm/program/program.hpp"

std::expected<vm::low::LowVMProgram, std::string>
	vm::Preprocessor::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

namespace {
	template<class Instruction>
	vm::program::VmInstruction getInstructionImpl(CRef<vm::parser::OpCode> opcode);

#define HANDLE_OPCODE_0ARGS(opcode)                                                        \
	template<>                                                                             \
	vm::program::VmInstruction getInstructionImpl<vm::program::instructions::Op_##opcode>( \
		CRef<vm::parser::OpCode> opcode                                                    \
	) {                                                                                    \
		CORE_ASSERT(opcode->args.size() == 0, "Invalid number of args");                   \
		return vm::program::instructions::Op_##opcode();                                   \
	}

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                             \
	template<>                                                                             \
	vm::program::VmInstruction getInstructionImpl<vm::program::instructions::Op_##opcode>( \
		CRef<vm::parser::OpCode> opcode                                                    \
	) {                                                                                    \
		CORE_ASSERT(opcode->args.size() == 1, "Invalid number of args");                   \
		if (std::holds_alternative<arg0_type>(opcode->args.at(0).arg)) {                   \
			return vm::program::instructions::Op_##opcode{                                 \
				.arg0 = std::get<arg0_type>(opcode->args.at(0).arg)                        \
			};                                                                             \
		}                                                                                  \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                    \
	}

#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                                  \
	template<>                                                                             \
	vm::program::VmInstruction getInstructionImpl<vm::program::instructions::Op_##opcode>( \
		CRef<vm::parser::OpCode> opcode                                                    \
	) {                                                                                    \
		CORE_ASSERT(opcode->args.size() == 2, "Invalid number of args");                   \
		if (std::holds_alternative<arg0_type>(opcode->args.at(0).arg)                      \
		    && std::holds_alternative<arg1_type>(opcode->args.at(1).arg)) {                \
			return vm::program::instructions::Op_##opcode{                                 \
				.arg0 = std::get<arg0_type>(opcode->args.at(0).arg),                       \
				.arg1 = std::get<arg1_type>(opcode->args.at(1).arg)                        \
			};                                                                             \
		}                                                                                  \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                    \
	}

#include <vm/program/opcodes_list.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE(opcode) \
	std::                     \
		make_pair(base::StrID(#opcode), getInstructionImpl<vm::program::instructions::Op_##opcode>),

	std::unordered_map instr_to_factory{
#include <vm/program/opcodes_list.hpp>
	};

#undef HANDLE_OPCODE

	vm::program::VmInstruction getInstruction(CRef<vm::parser::OpCode> opcode) {
		return instr_to_factory.at(opcode->opcode_name)(opcode);
	}

	std::expected<vm::program::Program, void> makeProgram(const std::vector<vm::program::CodeFile>& program, dia::Logger& log) {
		base::Map<base::StrID, vm::TypeRef> type_map;

		std::vector<Ref<Type>> good_types;

		for (auto& type: program.types) {
			base::StrID name = VISIT(type, value, return value.name);

			if (type_map.contains(name)) {
				auto msg = makeBox<vm::parser::DuplicatedTypeError>(*type->position);

				auto duplicated_types
					= program->types | std::views::filter([&](auto&& duplicated_type) {
						  base::StrID other_name
							  = VISIT(duplicated_type->datatype, value, return value.name);
						  return name == other_name && (&*duplicated_type != &*type);
					  });

				for (auto& duplicated_type: duplicated_types)
					msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(*duplicated_type->position)
					);

				log.log(std::move(msg));
			} else {
				vm::Type typ      = vm::Type::declareType(name);
				auto     type_ref = program->type_metadata->addType(std::move(typ));
				type_map.put(name, type_ref);
				good_types.push_back(type.refMut());
			}
		}

		for (auto& type: good_types) {
			variant_match(type->datatype) {
				variant_case(PrimitiveType, data) {
					type_map[data.name]->definePrimitive(data.size);
				}
				variant_case(PointerType, data) {
					type_map[data.name]->definePointer(type_map[data.inner]);
				}
				variant_case(StaticTableType, data) {
					type_map[data.name]->defineStaticTable(type_map[data.inner], data.table_size);
				}
				variant_case(DynamicTableType, data) {
					type_map[data.name]->defineDynamicTable(type_map[data.inner]);
				}
				variant_case(DataType, data) {
					std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
					fields.reserve(data.fields.size());
					for (auto& field: data.fields)
						fields.emplace_back(field.name, type_map[field.type]);
					type_map[data.name]->defineData(fields);
				}
				variant_case(VariantType, data) {
					std::vector<vm::TypeRef> variants;
					variants.reserve(data.variant_alternatives.size());
					for (auto& variant: data.variant_alternatives)
						variants.emplace_back(type_map[variant]);
					type_map[data.name]->defineVariant(variants);
				}
				variant_case(FunctionType, data) {
					std::vector<vm::TypeCRef> parameters;
					parameters.reserve(data.parameters.size());
					for (auto& param: data.parameters) parameters.emplace_back(type_map[param]);
					type_map[data.name]->defineFunction(parameters, type_map[data.result]);
				}
				variant_default { CORE_PANIC("bad type"); }
			}
		}

		program->type_metadata->finalize();
	}
}

std::expected<vm::low::LowVMProgram, std::string>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(err);
		opt_some(parsed_files) {
			base::HashMap<
				std::variant<program::Function*, program::VmInstruction*, opargs::OpCodeArg*>,
				dia::SourcePosition>
				pos_map;

			std::vector<program::CodeFile> program_files;
			for (const auto& parsed_file: parsed_files) {
				program::builders::CodeFileBuilder file_builder;
				for (const auto& tp: parsed_file.types) file_builder.addType(tp->datatype);

				for (const auto& func: parsed_file.functions) {
					program::builders::FunctionBuilder func_builder(
						func->name, file_builder.getAvailableTypes()
					);
					for (const auto& instr: func->code->opcodes)
						func_builder.addInstruction(getInstruction(instr.ref()));

					file_builder.addFunction(func_builder);
				}

				program_files.emplace_back(file_builder.build());
				// Fill the source_position map
				// for (const auto& tp: parsed_file.types) file_builder.addType(tp->datatype);
				// pos_map.put(program_files.back().ref().get(), parsed_file.source_file);
			}
			program::Program program;
			for (const auto& code_file: program_files) {
				for (const auto& tp: code_file.types) {
					base::StrID type_name = VISIT(tp, tp2, return tp2.name;);
					auto [_, inserted]    = program.types.insert_or_assign(type_name, tp);
					CORE_ASSERT(inserted, "");
				}
				for (const auto& func: code_file.functions) {
					program.functions.push_back(func);
					// Ignoring duplicated functions
					program.func_name_to_func_idx.put(func.name, program.functions.back());
				}
			}

			// auto is_valid = validator::verify(maybe_parsed_program.value());
			// if (is_valid.has_value()) return std::unexpected(is_valid.value());

			// auto program = vm::changeParsedProgramToLowVMProgram(&*maybe_parsed_program);
			// if (!program) return std::unexpected(program.error());

			// return program_files;
		}
	}
	CORE_UNREACHABLE();
}

vm::Preprocessor::Preprocessor(bool validate_program): validate_program(validate_program) {}
