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
		instr.bytecode_pos = *opcode.position;                            \
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
			instr.bytecode_pos = *opcode.position;                                             \
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
			instr.bytecode_pos = *opcode.position;                                                     \
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

	void insertGlobalData(
		const vm::code::GlobalData&        global,
		vm::code::builders::GlobalDataMap& globals_map,
		LoaderLogger&                      log
	) {
		if (globals_map.contains(global.name))
			log.log<DuplicatedGlobalDataError>(global.name, global.name.str);
		globals_map.insert(global, global.name);
	}
}

std::expected<Program, LoaderLogger> Program::from(const code::CodeCollection& code_collection) {
	Program      program;
	LoaderLogger log;

	program.insertTypes(code_collection.types, log);
	program.insertFunctions(code_collection.functions, log);
	program.insertGlobals(code_collection.global_data, log);
	if (!log.good()) return std::unexpected(std::move(log));
	return program;
}

void Program::insertCode(
	const std::vector<code::CodeCollection>& code_collections, LoaderLogger& logger
) {
	for (const auto& code_collection: code_collections) {
		insertTypes(code_collection.types, logger);
		insertFunctions(code_collection.functions, logger);
		insertGlobals(code_collection.global_data, logger);
	}
}

void Program::insertTypes(const std::vector<code::TypeOfData>& new_types, LoaderLogger& logger) {
	for (const auto& type: new_types) insertType(type, type_context_builder, logger);
}

void Program::insertFunctions(
	const std::vector<code::Function>& new_functions, LoaderLogger& logger
) {
	for (const auto& func: new_functions) {
		if (const auto func_name = func.name.str; functions.contains(func_name)) {
			logger.logMap<DuplicatedFunctionError>(func.name, [&](auto& err) {
				const auto dup_func = functions.at(func_name);
				logger.addNote<DuplicatedFunctionNote>(err, dup_func->name);
			});
		} else {
			functions.insert(func, func_name);
		}
	}
}

void Program::insertGlobals(const std::vector<code::GlobalData>& new_globals, LoaderLogger& logger) {
	for (const auto& global: new_globals) insertGlobalData(global, globals_map, logger);
}

Box<vm::TypeMetadata> Program::produceTypeMetadata() const {
	return std::move(type_context_builder.build()).moveMetadata();
}

const vm::StableTypeIdNameMap<vm::code::Function>& Program::funcMap() const { return functions; }

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& Program::typeMap() const {
	return type_context_builder.getTypes();
}

const vm::code::builders::GlobalDataMap& vm::loader::Program::globalMap() const {
	return globals_map;
}

Loader::Loader(const bool validate_program): validate_program(validate_program) {}

std::expected<vm::code::CodeCollection, LoaderLogger> Loader::loadFiles(
	const std::vector<fs::FilePath>& files
) {
	match_optional(parser::parse(files)) {
		opt_err(err) return std::unexpected(std::move(err));
		opt_some(parsed_files) {
			auto                          type_context_builder = vm::code::getBuiltinTypes();
			code::builders::GlobalDataMap globals;
			LoaderLogger                  log;
			for (const auto& parsed_file: parsed_files) {
				for (const auto& global: parsed_file.global_data) {
					auto code_global
						= code::GlobalData{ .name = global->name, .type = global->type };
					insertGlobalData(code_global, globals, log);
				}
				for (const auto& tp: parsed_file.types)
					insertType(tp->datatype, type_context_builder, log);
			}

			try {
				auto                        type_context = type_context_builder.build();
				std::vector<code::Function> functions;

				for (const auto& parsed_file: parsed_files) {
					for (const auto& func: parsed_file.functions) {
						code::Identifier func_name;
						func_name.str          = func->name.value;
						func_name.bytecode_pos = func->name.position;
						code::builders::FunctionBuilder func_builder(
							func_name, globals, type_context
						);

						for (const auto& instr: func->code->opcodes)
							func_builder.addInstruction(translateInstruction(*instr));

						functions.emplace_back(func_builder.build());
					}
				}
				if (log.good())
					return code::CodeCollection{
						.functions   = functions,
						.types       = type_context.getTypes() | std::ranges::to<std::vector>(),
						.global_data = globals | std::ranges::to<std::vector>(),
					};
			} catch (vm::code::builders::StackStructureMismatchError& e) {
				log.logMap<SomeBuilderError>(
					e.label,
					[&](Box<SomeBuilderError>& err) {
						for (const auto& instruction: e.jumps)
							log.addNote<SomeBuilderNote>(err, instruction, e.NOTE_MSG);
					},
					e.what()
				);
			} catch (code::builders::BuilderError& e) {
				match_optional(e.maybeElement()) {
					opt_some(elem) { log.log<SomeBuilderError>(*elem, e.what()); }
					opt_none { log.logSimple(e.what()); }
				}
			}
			return std::unexpected(std::move(log));
		}
	}
	CORE_UNREACHABLE();
}

std::expected<vm::low::LowVMProgram, LoaderLogger> Loader::getProgram(
	const std::vector<code::CodeCollection>& code_collection
) {
	LoaderLogger logger;
	program.insertCode(code_collection, logger);
	if (logger.good()) return compiler::compile(program);
	return std::unexpected(std::move(logger));
}

std::expected<vm::low::LowVMProgram, LoaderLogger> Loader::getProgram(
	const std::vector<fs::FilePath>& file_paths
) {
	auto opt_code_collection = loadFiles(file_paths);
	if (opt_code_collection.has_value())
		return getProgram({ std::move(opt_code_collection).value() });
	return std::unexpected(std::move(opt_code_collection).error());
}
