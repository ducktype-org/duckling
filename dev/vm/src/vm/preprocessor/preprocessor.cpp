#include "preprocessor.hpp"

#include "parser/elements.hpp"
#include "parser/errors.hpp"
#include "parser/parser.hpp"

#include <diagnostic/logger.hpp>

#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/code/builders/builders.hpp>
#include <vm/code/builders/errors.hpp>
#include <vm/code/code.hpp>
#include <vm/code/element_base.hpp>
#include <vm/code/instructions.hpp>
#include <vm/code/opcode_args.hpp>
#include <vm/code/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/preprocessor/compiler/compiler.hpp>
#include <vm/preprocessor/errors.hpp>
#include <vm/preprocessor/validator/validator.hpp>

#include <expected>
#include <unordered_map>
#include <variant>
#include <vector>

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const fs::FilePath& file) {
	return getProgram(std::vector{ file });
}

namespace {

	template<class Instruction>
	vm::code::Instruction getInstructionImpl(CRef<vm::parser::OpCode> opcode);

#define HANDLE_OPCODE_0ARGS(opcode)                                       \
	template<>                                                            \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>( \
		CRef<vm::parser::OpCode> opcode                                   \
	) {                                                                   \
		CORE_ASSERT(opcode->args.size() == 0, "Invalid number of args");  \
		auto instr         = VM_INSTR_FROM_NAME(opcode)();                \
		instr.bytecode_pos = *opcode->position;                           \
		return instr;                                                     \
	}

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                                  \
	template<>                                                                                  \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                       \
		CRef<vm::parser::OpCode> opcode                                                         \
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
		CRef<vm::parser::OpCode> opcode                                                         \
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

	void insertInstruction(
		CRef<vm::parser::OpCode>             opcode,
		vm::code::builders::FunctionBuilder& func_builder,
		vm::PreprocessorLogger&              logger
	) {
		auto instr = translateInstruction(opcode);
		try {
			func_builder.addInstruction(instr);
		} catch (vm::code::builders::StackStructureMismatchError& e) {
			logger.logMap<vm::preprocessor::StackStructureMismatchError>(
				VISIT(instr, in, return static_cast<const vm::code::ElementBase&>(in)),
				[&](Box<vm::preprocessor::StackStructureMismatchError>& err) {
					for (const auto& instruction: e.linked_instructions)
						err->addNote(makeBox<vm::preprocessor::StackStructureMismatchNote>(VISIT(
							instruction, in, return in.bytecode_pos.expect("No bytecode position")
						)));
				}
			);
		} catch (vm::code::builders::UnknownTypeError& e) {
			logger.log<vm::preprocessor::UnknownType>(e.TYPE, e.TYPE.type_name);
		}
	}

	void insertType(
		const vm::code::TypeOfData&         type,
		vm::code::builders::TypesContext<>& types,
		vm::PreprocessorLogger&             log
	) {
		using namespace vm;
		try {
			types.addType(type);
		} catch (code::builders::DuplicatedTypeError&) {
			base::StrID name = VISIT(type, tp, return tp.name);
			auto base = VISIT(type, value, return static_cast<const vm::code::ElementBase&>(value));
			log.logMap<preprocessor::DuplicatedTypeError>(base, [&](auto& msg) {
				for (const code::TypeOfData& duplicated_type: types.getTypes()) {
					base::StrID other_name = VISIT(duplicated_type, value, return value.name);
					if (name == other_name) {
						msg->addNote(makeBox<preprocessor::DuplicatedTypeNote>(
							VISIT(duplicated_type, tp, return tp.bytecode_pos.value())
						));
					}
				}
			});
		}
	}
}

void vm::Program::insertFunctions(
	const std::vector<code::Function>& new_functions, PreprocessorLogger& logger
) {
	for (const auto& func: new_functions) {
		auto func_name = func.name;
		if (functions.contains(func_name)) {
			logger.logMap<vm::preprocessor::DuplicateFunctionDefinitionError>(func, [&](auto& msg) {
				auto dup_func = functions.at(func_name);
				msg->addNote(makeBox<vm::preprocessor::DuplicatedFunctionDefinitionNote>(
					dup_func->bytecode_pos.value()
				));
			});
		} else {
			functions.insert(func, func_name);
		}
	}
}

std::expected<vm::Program, vm::PreprocessorLogger> vm::Program::from(
	const std::vector<vm::code::Function>& functions, const std::vector<code::TypeOfData>& types
) {
	vm::Program            program;
	vm::PreprocessorLogger log;

	program.insertTypes(types, log);
	program.insertFunctions(functions, log);
	if (!log.good()) return std::unexpected(std::move(log));
	return program;
}

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger>
	vm::Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some_move(parsed_files) {
			code::builders::TypesContext<> types_context_adding;
			vm::PreprocessorLogger         log;
			for (const auto& parsed_file: parsed_files)
				for (const auto& tp: parsed_file.types)
					insertType(tp->datatype, types_context_adding, log);

			try {
				auto                        types_context = types_context_adding.finalized();
				std::vector<code::Function> functions;

				for (const auto& parsed_file: parsed_files) {
					for (const auto& func: parsed_file.functions) {
						code::builders::FunctionBuilder func_builder(func->name, types_context);

						for (const auto& instr: func->code->opcodes)
							insertInstruction(instr.ref(), func_builder, log);

						func_builder.setRetSize(func->ret_size
						);  // @note: this is temporary, since function meta-parameters will be
						    // removed

						functions.emplace_back(func_builder.build());
					}
				}
				if (log.good()) return getProgram(functions, types_context.getTypes());
			} catch (code::builders::MissingSubtypeError& e) {
				log.log<preprocessor::MissingSubtypeError>(
					VISIT(e.BASE_TYPE, data, return static_cast<const code::ElementBase&>(data)),
					e.MISSING_NAME
				);
			}
			return std::unexpected(std::move(log));
		}
	}
	CORE_UNREACHABLE();
}

vm::Preprocessor::Preprocessor(bool validate_program): validate_program(validate_program) {}

std::expected<vm::low::LowVMProgram, vm::PreprocessorLogger> vm::Preprocessor::getProgram(
	const std::vector<code::Function>& functions, const std::vector<code::TypeOfData>& types
) {
	std::expected<Program, PreprocessorLogger> opt_program = Program::from(functions, types);
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

const base::StableTypeIdNameMap<vm::code::Function>& vm::Program::funcMap() const {
	return functions;
}

void vm::Program::insertTypes(
	const std::vector<code::TypeOfData>& new_types, vm::PreprocessorLogger& logger
) {
	for (const auto& type: new_types) insertType(type, types_context_adding, logger);
	auto finalized = types_context_adding.finalized();
	types          = finalized.getTypes();
	type_metadata  = std::move(finalized).moveMetadata();
}

Box<vm::TypeMetadata> vm::Program::produceTypeMetadata() const {
	return std::move(types_context_adding.finalized()).moveMetadata();
}
