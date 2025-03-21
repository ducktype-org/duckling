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
#include "parser/parser.hpp"
#include <vm/program/opcode_args.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <expected>
#include <vector>
#include "parser/elements.hpp"
#include "parser/errors.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include "vm/preprocessor/validator/validator.hpp"
#include "vm/program/builders/builders.hpp"
#include "vm/program/instructions.hpp"
#include "vm/program/program.hpp"
#include "vm/preprocessor/compiler/compiler.hpp"

std::expected<vm::low::LowVMProgram, dia::Logger>
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

	std::expected<vm::program::Program, dia::Logger> makeProgram(
		const std::vector<vm::program::CodeFile>& code_files,
		base::Optional<const vm::PosMap&>         pos_map
	) {
		using namespace vm::parser;
		vm::program::Program                      program;
		dia::Logger                               log;
		std::vector<Ref<vm::program::TypeOfData>> good_types;
		for (const auto& code_file: code_files) {
			for (const auto& type: code_file.types) {
				base::StrID name = VISIT(type, value, return value.name);

				if (program.types.contains(name)) {
					match_optional(pos_map) {
						opt_none return std::unexpected(dia::Logger());
						opt_some(map) {
							auto msg = makeBox<vm::parser::DuplicatedTypeError>(map.at(&type));

							for (auto& duplicated_type: good_types) {
								base::StrID other_name
									= VISIT(*duplicated_type, value, return value.name);
								if (name == other_name) {
									msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(
										map.at(duplicated_type.get())
									));
								}
							}

							log.log(std::move(msg));
						}
					}
				} else {
					vm::Type new_type = vm::Type::declareType(name);
					auto     type_ref = program.type_metadata->addType(std::move(new_type));
					good_types.emplace_back(Ref(&type));
					program.types.put(name, type_ref);
				}


				for (auto& func: code_file.functions) {
					auto func_name = func.name;
					if (program.functions.contains(func_name)) {
						match_optional(pos_map) {
							opt_none return std::unexpected(dia::Logger());
							opt_some(map) {
								auto msg = makeBox<vm::parser::DuplicateFunctionDefinitionError>(
									map.at(&func)
								);
								const auto& dup_func = program.functions.at(func_name);
								msg->addNote(makeBox<vm::parser::DuplicatedFunctionDefinitionNote>(
									map.at(&dup_func)
								));
								log.log(std::move(msg));
							}
						}
					}

					program.functions.put(func_name, func);
				}
			}
		}

		if (log.bad()) return std::unexpected(std::move(log));

		for (const auto& type: good_types) {
			variant_match(*type) {
				variant_case(vm::program::PrimitiveType, data) {
					program.types[data.name]->definePrimitive(data.size);
				}
				variant_case(vm::program::PointerType, data) {
					program.types[data.name]->definePointer(program.types[data.inner]);
				}
				variant_case(vm::program::StaticTableType, data) {
					program.types[data.name]->defineStaticTable(
						program.types[data.inner], data.table_size
					);
				}
				variant_case(vm::program::DynamicTableType, data) {
					program.types[data.name]->defineDynamicTable(program.types[data.inner]);
				}
				variant_case(vm::program::DataType, data) {
					std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
					fields.reserve(data.fields.size());
					for (auto& field: data.fields)
						fields.emplace_back(field.name, program.types[field.type]);
					program.types[data.name]->defineData(fields);
				}
				variant_case(vm::program::VariantType, data) {
					std::vector<vm::TypeRef> variants;
					variants.reserve(data.variant_alternatives.size());
					for (auto& variant: data.variant_alternatives)
						variants.emplace_back(program.types[variant]);
					program.types[data.name]->defineVariant(variants);
				}
				variant_case(vm::program::FunctionType, data) {
					std::vector<vm::TypeCRef> parameters;
					parameters.reserve(data.parameters.size());
					for (auto& param: data.parameters)
						parameters.emplace_back(program.types[param]);
					program.types[data.name]->defineFunction(
						parameters, program.types[data.result]
					);
				}
				variant_default { CORE_PANIC("bad type"); }
			}
		}

		program.type_metadata->finalize();

		return program;
	}
}

std::expected<vm::low::LowVMProgram, dia::Logger>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some_move(parsed_files) {
			PosMap pos_map;

			std::vector<program::CodeFile> code_files;
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

				code_files.emplace_back(file_builder.build());

				// Generate source positions
				for (const auto& [parsed_type, program_type]:
				     std::views::zip(parsed_file.types, code_files.back().types)) {
					pos_map.put(&program_type, parsed_type->position);
				}

				for (const auto& [parsed_func, func]:
				     std::views::zip(parsed_file.functions, code_files.back().functions)) {
					pos_map.put(&func, parsed_func->position);

					for (const auto& [parsed_instr, instr]:
					     std::views::zip(parsed_func->code->opcodes, func.body)) {
						pos_map.put(&instr, parsed_instr->position);

						variant_match(instr) {
#define HANDLE_OPCODE_0ARGS(opcode)

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                    \
	variant_case(vm::program::instructions::Op_##opcode, op) {    \
		pos_map.put(&op.arg0, parsed_instr->args.at(0).position); \
	}
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)         \
	variant_case(vm::program::instructions::Op_##opcode, op) {    \
		pos_map.put(&op.arg0, parsed_instr->args.at(0).position); \
		pos_map.put(&op.arg1, parsed_instr->args.at(1).position); \
	}
#include <vm/program/opcodes_list.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS
						}
					}
				}
			}
			return getProgram(code_files, pos_map);
		}
	}
	CORE_UNREACHABLE();
}

vm::Preprocessor::Preprocessor(bool validate_program): validate_program(validate_program) {}

std::expected<vm::low::LowVMProgram, dia::Logger> vm::Preprocessor::getProgram(
	const std::vector<program::CodeFile>& code_files, base::Optional<const PosMap&> pos_map
) {
	std::expected<program::Program, dia::Logger> opt_program = makeProgram(code_files, pos_map);
	if (opt_program.has_value()) {
		program::Program program = std::move(opt_program).value();

		auto validation_result = validator::verify(program, pos_map);
		if (!validation_result.has_value())
			return std::unexpected(std::move(validation_result).error());

		return vm::compiler::compile(std::move(program));
	} else
		return std::unexpected(std::move(opt_program).error());
}
