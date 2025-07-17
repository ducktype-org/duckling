#include "loader.hpp"

#include "parser/elements.hpp"
#include "parser/parser.hpp"

#include <diagnostic/logger.hpp>
#include <diagnostic/source_position.hpp>

#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/loader/compiler/compiler.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace vm::loader;

namespace {
	template<class Instruction>
	vm::code::Instruction getInstructionImpl(const parser::OpCode& opcode);

#define HANDLE_OPCODE_0ARGS(opcode)                                       \
	template<>                                                            \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>( \
		const parser::OpCode& opcode                                      \
	) {                                                                   \
		CORE_ASSERT(opcode.args.size() == 0, "Invalid number of args");   \
		auto instr         = VM_INSTR_FROM_NAME(opcode)();                \
		instr.bytecode_pos = opcode.position;                             \
		return instr;                                                     \
	}

#define HANDLE_OPCODE_1ARGS(opcode, arg0_type)                                                 \
	template<>                                                                                 \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                      \
		const parser::OpCode& opcode                                                           \
	) {                                                                                        \
		CORE_ASSERT(opcode.args.size() == 1, "Invalid number of args");                        \
		if (std::holds_alternative<arg0_type>(opcode.args.at(0))) {                            \
			auto instr = VM_INSTR_FROM_NAME(opcode){ std::get<arg0_type>(opcode.args.at(0)) }; \
			instr.bytecode_pos = opcode.position;                                              \
			return instr;                                                                      \
		}                                                                                      \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                        \
	}

#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type)                                              \
	template<>                                                                                         \
	vm::code::Instruction getInstructionImpl<VM_INSTR_FROM_NAME(opcode)>(                              \
		const parser::OpCode& opcode                                                                   \
	) {                                                                                                \
		CORE_ASSERT(opcode.args.size() == 2, "Invalid number of args");                                \
		if (std::holds_alternative<arg0_type>(opcode.args.at(0))                                       \
		    && std::holds_alternative<arg1_type>(opcode.args.at(1))) {                                 \
			auto instr         = VM_INSTR_FROM_NAME(opcode){ std::get<arg0_type>(opcode.args.at(0)),   \
				                                             std::get<arg1_type>(opcode.args.at(1)) }; \
			instr.bytecode_pos = opcode.position;                                                      \
			return instr;                                                                              \
		}                                                                                              \
		CORE_PANIC("Couldn't create opcode: " #opcode);                                                \
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

	vm::code::Instruction translateInstruction(const parser::OpCode& opcode) {
		return instr_to_factory.at(opcode.opcode_name.str())(opcode);
	}
}

std::expected<vm::code::CodeCollection, LoaderLogger> Loader::loadFiles(
	const std::vector<fs::File>& files
) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some(parsed_files) {
			code::CodeCollection new_code;

			for (const auto& parsed_file: parsed_files) {
				for (const auto& global: parsed_file.global_data) {
					code::GlobalData code_global;
					code_global.name         = global->name;
					code_global.type         = global->type;
					code_global.bytecode_pos = global->position;
					if (global->ctor_name.has_value())
						code_global.ctor_name = code::Identifier(global->ctor_name.value().value);

					if (global->dtor_name.has_value())
						code_global.dtor_name = code::Identifier(global->dtor_name.value().value);
					new_code.global_data.emplace_back(code_global);
				}
				for (const auto& tp: parsed_file.types) new_code.types.push_back(tp->datatype);

				for (const auto& func: parsed_file.functions) {
					code::Identifier func_name;
					func_name.str          = func->name.value;
					func_name.bytecode_pos = func->name.position;
					code::Function function;

					function.name         = func_name;
					function.bytecode_pos = func->position;

					for (const auto& instr: func->code->opcodes)
						function.body.push_back(translateInstruction(*instr));

					new_code.functions.push_back(std::move(function));
				}
			}

			return new_code;
		}
	}

	CORE_UNREACHABLE();
}

std::expected<vm::low::LowVMProgram, LoaderLogger> Loader::getProgram(
	const std::vector<code::CodeCollection>& code_collections
) {
	LoaderLogger log;
	try {
		for (const auto& code: code_collections) program = program.newInsertCode(code);
		return compiler::compile(program);
	} catch (code::StackStructureMismatchError& e) {
		log.logMap<SomeValidationError>(
			e.label,
			[&](Box<SomeValidationError>& err) {
				for (const auto& instruction: e.jumps)
					log.addNote<SomeValidationNote>(err, instruction, e.NOTE_MSG);
			},
			e.what()
		);
	} catch (code::DuplicatedFunctionError& e) {
		log.logMap<DuplicatedFunctionError>(e.NEW_ELEMENT, [&](auto& err) {
			log.addNote<DuplicatedFunctionNote>(err, e.PREVIOUS_ELEMENT);
		});
	} catch (code::DuplicatedGlobalDataError& e) {
		log.logMap<DuplicatedGlobalDataError>(
			e.NEW_ELEMENT,
			[&](auto& err) { log.addNote<DuplicatedGlobalDataNote>(err, e.PREVIOUS_ELEMENT); },
			e.NEW_ELEMENT.name.str
		);
	} catch (code::DuplicatedTypeError& e) {
		log.logMap<DuplicatedTypeError>(
			**e.maybeElement(),
			[&](auto& err) { log.addNote<DuplicatedTypeNote>(err, e.PREVIOUS_ELEMENT); },
			code::typeName(e.NEW_ELEMENT)
		);
	} catch (code::ValidationError& e) {
		match_optional(e.maybeElement()) {
			opt_some(elem) log.log<SomeValidationError>(*elem, e.what());
			opt_none log.logSimple(e.what());
		}
	}
	return std::unexpected(std::move(log));
}

std::expected<vm::low::LowVMProgram, LoaderLogger> Loader::getProgram(
	const std::vector<fs::File>& file_paths
) {
	auto opt_code_collection = loadFiles(file_paths);
	if (opt_code_collection.has_value()) return getProgram({ *std::move(opt_code_collection) });
	return std::unexpected(std::move(opt_code_collection).error());
}
