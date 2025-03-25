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
	std::vector<CRef<code::TypeOfData>> good_types;
	for (const auto& code_file: code_files) {
		for (const auto& type: code_file.types) {
			base::StrID name = VISIT(type, value, return value.name);
			auto base = VISIT(type, value, return static_cast<const vm::code::ElementBase&>(value));
			if (types.atMaybe(name).has_value()) {
				logger.logMap<parser::DuplicatedTypeError>(base, [&](auto& msg) {
					for (CRef<vm::code::TypeOfData> duplicated_type: good_types) {
						base::StrID other_name = VISIT(*duplicated_type, value, return value.name);
						if (name == other_name) {
							msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(
								VISIT(*duplicated_type, tp, return tp.bytecode_pos.value())
							));
						}
					}
				});
			} else {
				Type new_type = Type::declareType(name);
				types.addType(std::move(new_type));
				meta_types.insert(type, name);
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

void vm::Program::defineTypes() {
	for (const auto& type: meta_types) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				types.at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				types.at(data.name)->definePointer(types.at(data.inner));
			}
			variant_case(vm::code::StaticTableType, data) {
				types.at(data.name)->defineStaticTable(types.at(data.inner), data.table_size);
			}
			variant_case(vm::code::DynamicTableType, data) {
				types.at(data.name)->defineDynamicTable(types.at(data.inner));
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(field.name, types.at(field.type));
				types.at(data.name)->defineData(fields);
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(types.at(variant));
				types.at(data.name)->defineVariant(variants);
			}
			variant_case(vm::code::FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters) parameters.emplace_back(types.at(param));
				types.at(data.name)->defineFunction(parameters, types.at(data.result));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	types.finalize();
}

std::expected<vm::Program, vm::PreprocessorLogger>
	vm::Program::from(const std::vector<vm::code::CodeFile>& code_files) {
	vm::Program            program;
	vm::PreprocessorLogger log;

	program.insertTypesAndFunctions(code_files, log);
	if (!log.good()) return std::unexpected(std::move(log));
	program.defineTypes();
	return program;
}

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some_move(parsed_files) {
			std::vector<code::CodeFile> code_files;
			for (const auto& parsed_file: parsed_files) {
				code::builders::CodeFileBuilder file_builder;
				for (const auto& tp: parsed_file.types) file_builder.addType(tp->datatype);

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
