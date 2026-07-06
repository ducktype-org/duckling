#include "loader.hpp"

#include "parser/elements.hpp"
#include "parser/parser.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/vector_utils.hpp>
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
					code_global.is_constant  = global->is_constant;
					if (global->ctor_name.has_value())
						code_global.ctor_name = code::Identifier(global->ctor_name.value().value);

					if (global->dtor_name.has_value())
						code_global.dtor_name = code::Identifier(global->dtor_name.value().value);
					if (global->initial_value.has_value())
						code_global.initial_value = global->initial_value.value();
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

				for (const auto& ffi_func: parsed_file.ffi_functions) {
					code::FFIFunction function;
					function.bytecode_pos = ffi_func->position;

					code::Identifier func_name;
					func_name.str          = ffi_func->name.value;
					func_name.bytecode_pos = ffi_func->name.position;
					function.name          = func_name;

					for (const auto& param: ffi_func->parameters) {
						code::Identifier param_id;
						param_id.str          = param.value;
						param_id.bytecode_pos = param.position;
						function.signature.parameters.emplace_back(param_id);
					}

					for (const auto& reslt: ffi_func->result_types) {
						code::Identifier result_type_id;
						result_type_id.str          = reslt.value;
						result_type_id.bytecode_pos = reslt.position;
						function.signature.result_types.emplace_back(result_type_id);
					}

					new_code.ffi_functions.emplace_back(function);
				}

				for (const auto& ffi_object: parsed_file.ffi_objects) {
					fs::FilePath path{ ffi_object->path.strView() };
					// A name without a directory component (e.g. "libm.so.6") is passed through
					// to dlopen, which searches the system library paths.
					if (!path.getPath().has_parent_path()) {
						new_code.object_files.emplace_back(path.string());
						continue;
					}
					if (!path.getPath().is_absolute())
						path = parsed_file.source_file.getFilePath().parentPath() / path;
					if (!path.exists()) {
						LoaderLogger log;
						log.logSimple(base::strConcat(
							"Failed to load `ffi object` file: ", path.string(), ": file not found."
						));
						return std::unexpected(std::move(log));
					}
					new_code.object_files.emplace_back(path.string());
				}
			}
			base::deduplicateBy(new_code.functions, [](const code::Function& func) {
				return func.name.str.strView();
			});
			base::deduplicateBy(
				new_code.external_c_functions,
				[](const code::ExternalCFunction& func) { return func.name.str.strView(); }
			);
			base::deduplicateBy(new_code.ffi_functions, [](const code::FFIFunction& func) {
				return func.name.str.strView();
			});
			base::deduplicateBy(new_code.object_files, [](const std::string& file) { return file; });
			base::deduplicateBy(new_code.global_data, [](const vm::code::GlobalData& g) {
				return g.name.str.strView();
			});
			base::deduplicateBy(new_code.types, [](const vm::code::TypeOfData& f) {
				return typeName(f);
			});

			return new_code;
		}
	}

	CORE_UNREACHABLE();
}

std::expected<void, LoaderLogger> Loader::loadAndValidate(const code::CodeCollection& code_collection
) {
	// Skip if no new code was added.
	if (code_collection.functions.empty() && code_collection.types.empty()
	    && code_collection.global_data.empty() && code_collection.external_c_functions.empty()
	    && code_collection.ffi_functions.empty() && code_collection.object_files.empty()) {
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
	} catch (code::DuplicatedFFIFunctionError& e) {
		log.logMap(
			e.new_element,
			[&](auto& err) {
				log.addNote(err, e.previous_element, "Previous FFI function declaration here.");
			},
			"FFI function with this name already exists."
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

base::Optional<FatBytecodePosition> vm::loader::Loader::mapFileLineToCodeCollectionPosition(
	const fs::File& file, usize line
) const {
	for (const auto& function: getHighProgram()->functions()) {
		// ensure function has position data and is in requested file
		if (!function.bytecode_pos) continue;
		if (function.bytecode_pos->getSource()->getFile() != file) continue;

		const auto& body = function.body;

		// Return first instruction if line contains function name
		if (function.name.bytecode_pos->getStartLineColumn().first == line)
			return FatBytecodePosition{
				.function_name     = function.name.str,
				.instruction_index = 0,
			};

		auto get_pos = [](auto&& i) { return i.bytecode_pos; };

		// Skip if before or after the function
		if (body.front().visit(get_pos)->getStartLineColumn().first > line) continue;
		if (body.back().visit(get_pos)->getStartLineColumn().first < line) continue;

		// Binsearch line
		auto guess = std::ranges::lower_bound(
			body,
			line,
			std::ranges::less{},
			[&](const auto& instr) -> usize {
				return instr.visit(get_pos)->getStartLineColumn().first;
			}
		);

		if (guess == body.end())
			break;  // this line is between this function instructions, so it's not in any other function
		auto pos = guess->visit(get_pos);

		if (pos->getSource()->getFile() == file && pos->getStartLineColumn().first == line)
			return FatBytecodePosition{
				.function_name     = function.name.str,
				.instruction_index = static_cast<usize>(std::distance(body.begin(), guess)),
			};
	}

	return std::nullopt;
}
