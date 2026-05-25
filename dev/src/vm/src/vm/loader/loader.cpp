#include "loader.hpp"

#include "parser/elements.hpp"
#include "parser/parser.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/source_position.hpp>
#include <string_id/string_id.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_program.hpp>
#include <vm/loader/logger.hpp>

#include <vector>

using namespace vm::loader;

namespace {
	/**
	 * @brief Translates a parsed opcode (`parser::OpCode`) into a high-level bytecode instruction
	 * (`vm::code::Instruction`).
	 */
	vm::code::Instruction translateInstruction(const parser::OpCode& opcode) {
		auto instruction
			= vm::code::builders::makeInstructionFromArgs(opcode.opcode_name, opcode.args);
		instruction.visit([&](auto&& i) { i.bytecode_pos = opcode.position; });
		return instruction;
	}
}

std::expected<vm::code::CodeCollection, LoaderLogger> Loader::parseFiles(
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
					code::Function function;
					function.bytecode_pos = func->position;

					code::Identifier func_name;
					func_name.str          = func->name.value;
					func_name.bytecode_pos = func->name.position;
					function.name          = func_name;

					for (const auto& param: func->parameters) {
						code::Identifier param_id;
						param_id.str          = param.value;
						param_id.bytecode_pos = param.position;
						function.signature.parameters.emplace_back(param_id);
					}

					for (const auto& reslt: func->result_types) {
						code::Identifier result_type_id;
						result_type_id.str          = reslt.value;
						result_type_id.bytecode_pos = reslt.position;
						function.signature.result_types.emplace_back(result_type_id);
					}

					for (const auto& instr: func->code->opcodes)
						function.body.push_back(translateInstruction(*instr));

					new_code.functions.emplace_back(function);
				}
			}

			return new_code;
		}
	}

	CORE_UNREACHABLE();
}

std::expected<void, LoaderLogger> Loader::loadAndValidate(const code::CodeCollection& code_collection
) {
	// Skip if no new code was added.
	if (code_collection.functions.empty() && code_collection.types.empty()
	    && code_collection.global_data.empty() && code_collection.external_c_functions.empty()) {
		return {};
	}

	LoaderLogger log;
	try {
		// @note: This function creates a copy of the current program state and tries inserting new
		// code into it. If it fails, an exception is thrown and `validated_high_program` in the
		// loader stays unchanged.
		validated_high_program = validated_high_program.tryInsertCode(code_collection);

		return {};
	} catch (code::StackStructureMismatchError& e) {
		log.logMap(
			e.label,
			[&](Box<dia_int::PlaceholderError>& err) {
				for (const auto& instruction: e.jumps)
					instruction.visit([&](auto&& i) {
						log.addNote(
							err,
							static_cast<const code::ElementBase&>(i),
							code::StackStructureMismatchError::NOTE_MSG
						);
					});
			},
			e.what()
		);
	} catch (code::DuplicatedFunctionError& e) {
		log.logMap(
			e.new_element,
			[&](auto& err) {
				log.addNote(err, e.previous_element, "Previous function declaration here.");
			},
			"Function with this name already exists."
		);
	} catch (code::DuplicatedGlobalDataError& e) {
		log.logMap(
			e.new_element,
			[&](auto& err) { log.addNote(err, e.previous_element, "Previous declaration here."); },
			"Global variable with this name already exists."
		);
	} catch (code::DuplicatedTypeError& e) {
		log.logMap(
			**e.maybeElement(),
			[&](auto& err) {
				log.addNote(err, e.previous_element, "Previous type declaration here.");
			},
			"Type with this name already exists."
		);
	} catch (code::ValidationError& e) {
		match_optional(e.maybeElement()) {
			opt_some(elem) log.log(*elem, e.what());
			opt_none log.logSimple(e.what());
		}
	}
	return std::unexpected(std::move(log));
}

std::expected<void, LoaderLogger> Loader::loadAndValidate(const std::vector<fs::File>& file_paths) {
	auto opt_code_collection = parseFiles(file_paths);
	if (opt_code_collection.has_value()) return loadAndValidate(*opt_code_collection);
	return std::unexpected(std::move(opt_code_collection).error());
}

base::CRef<vm::code::ValidProgram> vm::loader::Loader::getHighProgram() const {
	return &validated_high_program;
}

std::expected<vm::code::CodeCollection, std::string> vm::loader::Loader::parseCodeCollectionFromFiles(
	const std::vector<fs::File>& files
) {
	return parseFiles(files).transform_error([](LoaderLogger logger) {
		std::stringstream ss;
		logger.dump(ss);
		return ss.str();
	});
}

std::expected<base::Optional<dia::SourcePosition>, vm::loader::MappingException> vm::loader::
	Loader::mapCodeCollectionPositionToFilePosition(FatBytecodePosition position) const {
	const auto& maybe_high_function = getHighProgram()->functions().atMaybe(position.function_name);
	if (maybe_high_function.empty()) return std::unexpected(MappingException::NoFunction);

	return maybe_high_function.value()->body.at(position.instruction_index).visit([](auto&& instr) {
		return instr.bytecode_pos;
	});
}
