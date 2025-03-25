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
#include "vm/program/type_of_data.hpp"

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

template<class Instruction>
vm::program::Instruction getInstructionImpl(CRef<vm::parser::OpCode> opcode);

#define HANDLE_OPCODE_0ARGS(opcode)                                          \
	template<>                                                               \
	vm::program::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>( \
		CRef<vm::parser::OpCode> opcode                                      \
	) {                                                                      \
		CORE_ASSERT(opcode->args.size() == 0, "Invalid number of args");     \
		return VM_INSTR_FROM_NAME(opcode)();                                 \
	}

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                            \
	template<>                                                                            \
	vm::program::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(              \
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
	vm::program::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(              \
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

#include <vm/program/opcodes_list.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE(opcode) \
	std::make_pair(base::StrID(#opcode), getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>),

std::unordered_map instr_to_factory{
#include <vm/program/opcodes_list.hpp>
};

#undef HANDLE_OPCODE

vm::program::Instruction translateInstruction(CRef<vm::parser::OpCode> opcode) {
	return instr_to_factory.at(opcode->opcode_name)(opcode);
}

void vm::Program::insertTypesAndFunctions(
	const std::vector<vm::program::CodeFile>& code_files, vm::PreprocessorLogger& logger
) {
	std::vector<CRef<vm::program::TypeOfData>> good_types;
	for (const auto& code_file: code_files) {
		for (const auto& type: code_file.types) {
			base::StrID name = VISIT(type, value, return value.name);
			auto        base
				= VISIT(type, value, return static_cast<const vm::program::ElementBase&>(value));
			if (types.atMaybe(name).has_value()) {
				logger.logMap<vm::parser::DuplicatedTypeError>(base, [&](auto& msg) {
					for (CRef<vm::program::TypeOfData> duplicated_type: good_types) {
						base::StrID other_name = VISIT(*duplicated_type, value, return value.name);
						if (name == other_name) {
							msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(
								VISIT(*duplicated_type, tp, return tp.bytecode_pos.value())
							));
						}
					}
				});
			} else {
				vm::Type new_type = vm::Type::declareType(name);
				types.addType(std::move(new_type));
				meta_types.insert(type, name);
			}

			for (auto& func: code_file.functions) {
				auto func_name = func.name;
				if (functions.contains(func_name)) {
					logger.logMap<vm::parser::DuplicateFunctionDefinitionError>(
						func,
						[&](auto& msg) {
							auto dup_func = functions.at(func_name);
							msg->addNote(makeBox<vm::parser::DuplicatedFunctionDefinitionNote>(
								dup_func->bytecode_pos.value()
							));
						}
					);
				} else {
					functions.insert(func, func_name);
				}
			}
		}
	}
}

void vm::Program::defineTypes() {
	for (const auto& type: meta_types) {
		variant_match(type) {
			variant_case(vm::program::PrimitiveType, data) {
				types.at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::program::PointerType, data) {
				types.at(data.name)->definePointer(types.at(data.inner));
			}
			variant_case(vm::program::StaticTableType, data) {
				types.at(data.name)->defineStaticTable(types.at(data.inner), data.table_size);
			}
			variant_case(vm::program::DynamicTableType, data) {
				types.at(data.name)->defineDynamicTable(types.at(data.inner));
			}
			variant_case(vm::program::DataType, data) {
				std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(field.name, types.at(field.type));
				types.at(data.name)->defineData(fields);
			}
			variant_case(vm::program::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(types.at(variant));
				types.at(data.name)->defineVariant(variants);
			}
			variant_case(vm::program::FunctionType, data) {
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
	vm::Program::from(const std::vector<vm::program::CodeFile>& code_files) {
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
			std::vector<program::CodeFile> code_files;
			for (const auto& parsed_file: parsed_files) {
				program::builders::CodeFileBuilder file_builder;
				for (const auto& tp: parsed_file.types) file_builder.addType(tp->datatype);

				for (const auto& func: parsed_file.functions) {
					program::builders::FunctionBuilder func_builder(
						func->name, file_builder.getAvailableTypes()
					);
					for (const auto& instr: func->code->opcodes)
						func_builder.addInstruction(translateInstruction(instr.ref()));

					file_builder.addFunction(func_builder);
				}

				code_files.emplace_back(file_builder.build());

				// Generate source positions
				// 				for (const auto& [parsed_type, program_type]:
				// 				     std::views::zip(parsed_file.types, code_files.back().types)) {
				// 					pos_map.put(&program_type, parsed_type->position);
				// 				}

				// 				for (const auto& [parsed_func, func]:
				// 				     std::views::zip(parsed_file.functions,
				// code_files.back().functions)) { 					pos_map.put(&func,
				// parsed_func->position);

				// 					for (const auto& [parsed_instr, instr]:
				// 					     std::views::zip(parsed_func->code->opcodes, func.body)) {
				// 						pos_map.put(&VISIT(instr, in, return &in),
				// parsed_instr->position);

				// 						variant_match(instr) {
				// #define HANDLE_OPCODE_0ARGS(opcode)

				// #define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                    \
// 	variant_case(VM_INSTR_FROM_NAME(opcode), op) {                \
// 		pos_map.put(&op.arg0, parsed_instr->args.at(0).position); \
// 	}
				// #define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)         \
// 	variant_case(VM_INSTR_FROM_NAME(opcode), op) {                \
// 		pos_map.put(&op.arg0, parsed_instr->args.at(0).position); \
// 		pos_map.put(&op.arg1, parsed_instr->args.at(1).position); \
// 	}
				// #include <vm/program/opcodes_list.hpp>

				// #undef HANDLE_OPCODE_0ARGS
				// #undef HANDLE_OPCODE_1ARGS
				// #undef HANDLE_OPCODE_2ARGS
				// 						}
				// 					}
				// 				}
			}
			return getProgram(code_files);
		}
	}
	CORE_UNREACHABLE();
}

vm::Preprocessor::Preprocessor(bool validate_program): validate_program(validate_program) {}

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const std::vector<program::CodeFile>& code_files) {
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

const base::StableTypeIdNameMap<vm::program::Function> vm::Program::funcMap() const {
	return functions;
}

const vm::TypeMetadata& vm::Program::getTypeMetadata() const { return types; }
