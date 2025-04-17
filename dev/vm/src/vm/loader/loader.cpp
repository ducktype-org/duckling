#include "loader.hpp"

#include "parser/elements.hpp"
#include "parser/parser.hpp"

#include <diagnostic/logger.hpp>

#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/loader/compiler/compiler.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/validator/validator.hpp>

#include <expected>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace vm::loader;

namespace {
	template<class Instruction>
	vm::code::Instruction getInstructionImpl(CRef<parser::OpCode> opcode);

#define HANDLE_OPCODE_0ARGS(opcode)                                       \
	template<>                                                            \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>( \
		CRef<parser::OpCode> opcode                                       \
	) {                                                                   \
		CORE_ASSERT(opcode->args.size() == 0, "Invalid number of args");  \
		auto instr         = VM_INSTR_FROM_NAME(opcode)();                \
		instr.bytecode_pos = *opcode->position;                           \
		return instr;                                                     \
	}

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                                  \
	template<>                                                                                  \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                       \
		CRef<parser::OpCode> opcode                                                             \
	) {                                                                                         \
		CORE_ASSERT(opcode->args.size() == 1, "Invalid number of args");                        \
		if (std::holds_alternative<arg0_type>(opcode->args.at(0))) {                            \
			auto instr = VM_INSTR_FROM_NAME(opcode){ std::get<arg0_type>(opcode->args.at(0)) }; \
			instr.bytecode_pos = *opcode->position;                                             \
			return instr;                                                                       \
		}                                                                                       \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                         \
	}

#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                                       \
	template<>                                                                                  \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                       \
		CRef<parser::OpCode> opcode                                                             \
	) {                                                                                         \
		CORE_ASSERT(opcode->args.size() == 2, "Invalid number of args");                        \
		if (std::holds_alternative<arg0_type>(opcode->args.at(0))                               \
		    && std::holds_alternative<arg1_type>(opcode->args.at(1))) {                         \
			auto instr = VM_INSTR_FROM_NAME(opcode){ std::get<arg0_type>(opcode->args.at(0)),   \
				                                     std::get<arg1_type>(opcode->args.at(1)) }; \
			instr.bytecode_pos = *opcode->position;                                             \
			return instr;                                                                       \
		}                                                                                       \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                         \
	}

#include <vm/bytecode/opcode_definitions.hpp>

#undef HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_2ARGS

#define HANDLE_OPCODE(opcode) \
	std::make_pair(std::string(#opcode), getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>),

	std::unordered_map instr_to_factory{
#include <vm/bytecode/opcode_definitions.hpp>
	};

#undef HANDLE_OPCODE

	vm::code::Instruction translateInstruction(CRef<parser::OpCode> opcode) {
		return instr_to_factory.at(opcode->opcode_name.str())(opcode);
	}

	void insertInstruction(
		CRef<parser::OpCode>                 opcode,
		vm::code::builders::FunctionBuilder& func_builder,
		LoaderLogger&                        logger
	) {
		auto instr = translateInstruction(opcode);
		try {
			func_builder.addInstruction(instr);
		} catch (vm::code::builders::StackStructureMismatchError& e) {
			logger.logMap<StackStructureMismatchError>(
				instr,
				[&](Box<StackStructureMismatchError>& err) {
					for (const auto& instruction: e.linked_instructions)
						logger.addNote<StackStructureMismatchNote>(err, instruction);
				}
			);
		} catch (vm::code::builders::UnknownTypeError& e) {
			logger.log<UnknownTypeError>(e.TYPE, e.TYPE.type_name);
		} catch (vm::code::builders::MissingFunctionalTypeError& e) {
			logger.log<UnknownFunctionError>(instr, e.FUNC_NAME);
		} catch (vm::code::builders::BuilderError& e) {
			logger.log<SomeBuilderError>(instr, e.what());
		}
	}

	void insertType(
		const vm::code::TypeOfData&             type,
		vm::code::builders::TypeContextBuilder& types,
		LoaderLogger&                           log
	) {
		using namespace vm;
		try {
			types.addType(type);
		} catch (code::builders::DuplicatedTypeError&) {
			base::StrID name = VISIT(type, tp, return tp.name);
			auto base = VISIT(type, value, return static_cast<const vm::code::ElementBase&>(value));
			log.logMap<DuplicatedTypeError>(
				base,
				[&](auto& err) {
					for (const code::TypeOfData& duplicated_type: types.getTypes()) {
						base::StrID other_name = VISIT(duplicated_type, value, return value.name);
						if (name == other_name)
							log.addNote<DuplicatedTypeNote>(err, duplicated_type, name);
					}
				},
				name
			);
		}
	}

	std::expected<void, LoaderLogger> validateMain(const Program& program) {
		LoaderLogger log;

		auto is_valid_main_return_type = [](const vm::code::TypeOfData& type) -> bool {
			variant_match(type) {
				variant_case(vm::code::PrimitiveType, primitive) { return primitive.size == 8; }
				variant_default { return false; }
			}
			return false;
		};

		// Check if main function exists
		auto opt_main = program.funcMap().atMaybe(base::StrID("main"));
		if (!opt_main.has_value()) {
			log.logSimple(NO_MAIN_ERR.data());
			return std::unexpected(std::move(log));
		}

		// Check if main type exists and is a function type
		// @note: We are guaranteed that a function type for each function exists. It's checked by
		// builders.
		auto main_type = *program.typeMap().atMaybe(base::StrID("main"));

		variant_match(*main_type) {
			variant_case(vm::code::FunctionType, func_type) {
				auto opt_return_type = program.typeMap().atMaybe(func_type.result);
				if (!opt_return_type || !is_valid_main_return_type(**opt_return_type)) {
					log.logSimple(WRONG_MAIN_RET_VAL_ERR.data());
					return std::unexpected(std::move(log));
				}
			}
		}

		return {};
	}
}

void Program::insertFunctions(
	const std::vector<code::Function>& new_functions, LoaderLogger& logger
) {
	for (const auto& func: new_functions) {
		if (const auto func_name = func.name; functions.contains(func_name)) {
			logger.logMap<DuplicatedFunctionError>(func, [&](auto& err) {
				const auto dup_func = functions.at(func_name);
				logger.addNote<DuplicatedFunctionNote>(err, *dup_func);
			});
		} else {
			functions.insert(func, func_name);
		}
	}
}

std::expected<Program, LoaderLogger> Program::from(const code::CodeCollection& code_collection) {
	Program      program;
	LoaderLogger log;

	program.insertTypes(code_collection.types, log);
	program.insertFunctions(code_collection.functions, log);
	if (!log.good()) return std::unexpected(std::move(log));
	return program;
}

std::expected<vm::low::LowVMProgram, LoaderLogger>
	Loader::getProgram(const std::vector<fs::FilePath>& files) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some_move(parsed_files) {
			auto         type_context_builder = vm::code::builtin_types::getBuiltinTypes();
			LoaderLogger log;
			for (const auto& parsed_file: parsed_files)
				for (const auto& tp: parsed_file.types)
					insertType(tp->datatype, *type_context_builder, log);

			try {
				auto                        type_context = type_context_builder->build();
				std::vector<code::Function> functions;

				for (const auto& parsed_file: parsed_files) {
					for (const auto& func: parsed_file.functions) {
						code::builders::FunctionBuilder func_builder(func->name, type_context);

						for (const auto& instr: func->code->opcodes)
							insertInstruction(instr.ref(), func_builder, log);

						functions.emplace_back(func_builder.build());
					}
				}
				if (log.good())
					return getProgram({ .functions = functions, .types = type_context.getTypes() });
			} catch (code::builders::MissingSubtypeError& e) {
				log.log<UnknownSubtypeError>(e.BASE_TYPE, e.MISSING_NAME);
			} catch (code::builders::BuilderError& e) { log.logSimple(e.what()); }
			return std::unexpected(std::move(log));
		}
	}
	CORE_UNREACHABLE();
}

Loader::Loader(const bool validate_program): validate_program(validate_program) {}

std::expected<vm::low::LowVMProgram, LoaderLogger> Loader::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

std::expected<vm::low::LowVMProgram, LoaderLogger>
	Loader::getProgram(const code::CodeCollection& code_collection) {
	std::expected<Program, LoaderLogger> opt_program = Program::from(code_collection);
	if (opt_program.has_value()) {
		const Program program = std::move(opt_program).value();

		auto main_validation = validateMain(program);
		if (!main_validation.has_value())
			return std::unexpected(std::move(main_validation).error());

		auto validation_result = validator::verify(program);
		if (!validation_result.has_value())
			return std::unexpected(std::move(validation_result).error());

		return compiler::compile(program);
	}
	return std::unexpected(std::move(opt_program).error());
}

const vm::StableTypeIdNameMap<vm::code::Function>& Program::funcMap() const { return functions; }

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& Program::typeMap() const {
	return type_context_builder.getTypes();
}

void Program::insertTypes(const std::vector<code::TypeOfData>& new_types, LoaderLogger& logger) {
	for (const auto& type: new_types) insertType(type, type_context_builder, logger);
}

Box<vm::TypeMetadata> Program::produceTypeMetadata() const {
	return std::move(type_context_builder.build()).moveMetadata();
}
