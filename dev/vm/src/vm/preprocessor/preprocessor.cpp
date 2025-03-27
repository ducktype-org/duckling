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
#include <vm/code/opcode_args.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <expected>
#include <vector>
#include "parser/elements.hpp"
#include "parser/errors.hpp"
#include "vm/core/process/type_metadata/type_metadata.hpp"
#include "vm/preprocessor/validator/validator.hpp"
#include "vm/code/builders/builders.hpp"
#include "vm/code/instructions.hpp"
#include "vm/code/code.hpp"
#include "vm/preprocessor/compiler/compiler.hpp"
#include "vm/code/type_of_data.hpp"

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

template<class Instruction>
vm::code::Instruction getInstructionImpl(CRef<vm::parser::OpCode> opcode);

#define HANDLE_OPCODE_0ARGS(opcode)                                       \
	template<>                                                            \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>( \
		CRef<vm::parser::OpCode> opcode                                   \
	) {                                                                   \
		CORE_ASSERT(opcode->args.size() == 0, "Invalid number of args");  \
		return VM_INSTR_FROM_NAME(opcode)();                              \
	}

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                            \
	template<>                                                                            \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                 \
		CRef<vm::parser::OpCode> opcode                                                   \
	) {                                                                                   \
		CORE_ASSERT(opcode->args.size() == 1, "Invalid number of args");                  \
		if (std::holds_alternative<arg0_type>(opcode->args.at(0))) {                      \
			return VM_INSTR_FROM_NAME(opcode){ std::get<arg0_type>(opcode->args.at(0)) }; \
		}                                                                                 \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                   \
	}

#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                                 \
	template<>                                                                            \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                 \
		CRef<vm::parser::OpCode> opcode                                                   \
	) {                                                                                   \
		CORE_ASSERT(opcode->args.size() == 2, "Invalid number of args");                  \
		if (std::holds_alternative<arg0_type>(opcode->args.at(0))                         \
		    && std::holds_alternative<arg1_type>(opcode->args.at(1))) {                   \
			return VM_INSTR_FROM_NAME(opcode){ std::get<arg0_type>(opcode->args.at(0)),   \
				                               std::get<arg1_type>(opcode->args.at(1)) }; \
		}                                                                                 \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                   \
	}

#include <vm/code/opcodes_list.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE(opcode) \
	std::make_pair(std::string(#opcode), getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>),

std::unordered_map instr_to_factory{
#include <vm/code/opcodes_list.hpp>
};

#undef HANDLE_OPCODE

vm::code::Instruction translateInstruction(CRef<vm::parser::OpCode> opcode) {
	return instr_to_factory.at(opcode->opcode_name.str())(opcode);
}

void vm::Program::insertTypesAndFunctions(
	const std::vector<code::CodeFile>& code_files, PreprocessorLogger& logger
) {
	vm::code::builders::Builder for (const auto& code_file: code_files) {
		for (const code::TypeOfData& type_of_data: code_file.types) {
			base::StrID name = VISIT(type_of_data, value, return value.name);
			if (types.atMaybe(name).has_value()) {
				auto base = VISIT(
					type_of_data, value, return static_cast<const vm::code::ElementBase&>(value)
				);
				logger.logMap<parser::DuplicatedTypeError>(base, [&](auto& msg) {
					for (const vm::code::TypeOfData& duplicated_type: meta_types) {
						base::StrID other_name = VISIT(duplicated_type, value, return value.name);
						if (name == other_name) {
							msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(
								VISIT(duplicated_type, tp, return tp.bytecode_pos.value())
							));
						}
					}
				});
			} else {
				Type type = Type::from(type_of_data);
				types.addType(std::move(type));
				meta_types.insert(type_of_data, name);
			}
		}

		for (auto& func: code_file.functions) {
			auto func_name = func.name;
			if (functions.contains(func_name)) {
				logger.logMap<vm::parser::DuplicateFunctionDefinitionError>(func, [&](auto& msg) {
					auto dup_func = functions.at(func_name);
					msg->addNote(makeBox<vm::parser::DuplicatedFunctionDefinitionNote>(
						dup_func->bytecode_pos.value()
					));
				});
			} else {
				functions.insert(func, func_name);
			}
		}
	}
}

std::expected<vm::Program, vm::PreprocessorLogger>
	vm::Program::from(const std::vector<vm::code::CodeFile>& code_files) {
	vm::Program            program;
	vm::PreprocessorLogger log;

	program.insertTypesAndFunctions(code_files, log);
	if (!log.good()) return std::unexpected(std::move(log));
	program.types.finalize();
	return program;
}

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some_move(parsed_files) {
			std::vector<code::CodeFile>     code_files;
			code::builders::CodeFileBuilder file_builder_adding;
			for (const auto& parsed_file: parsed_files)
				for (const auto& tp: parsed_file.types) file_builder_adding.addType(tp->datatype);
			code::builders::CodeFileBuilder file_builder = file_builder_adding.finalize();

			for (const auto& parsed_file: parsed_files) {
				for (const auto& func: parsed_file.functions) {
					code::builders::FunctionBuilder func_builder(
						func->name, file_builder.getAvailableTypes()
					);
					for (const auto& instr: func->code->opcodes)
						func_builder.addInstruction(translateInstruction(instr.ref()));

					file_builder.addFunction(func_builder);
				}

				code_files.emplace_back(file_builder.build());
			}
			return getProgram(code_files);
		}
	}
	CORE_UNREACHABLE();
}

vm::Preprocessor::Preprocessor(bool validate_program): validate_program(validate_program) {}

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const std::vector<code::CodeFile>& code_files) {
	std::expected<Program, PreprocessorLogger> opt_program = Program::from(code_files);
	if (opt_program.has_value()) {
		Program program = std::move(opt_program).value();

		auto validation_result = validator::verify(program);
		if (!validation_result.has_value())
			return std::unexpected(std::move(validation_result).error());

		return vm::compiler::compile(program);
	} else {
		return std::unexpected(std::move(opt_program).error());
	}
}

const base::StableTypeIdNameMap<vm::code::Function> vm::Program::funcMap() const {
	return functions;
}

const vm::TypeMetadata& vm::Program::getTypeMetadata() const { return types; }
