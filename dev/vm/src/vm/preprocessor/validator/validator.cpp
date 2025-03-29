#include "validator.hpp"
#include <algorithm>
#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <vector>
#include <vm/program/opcode_args.hpp>
#include <diagnostic/logger.hpp>
#include <base/string_id.hpp>
#include "base/ints.hpp"
#include "base/maps.hpp"
#include "errors.hpp"
#include "vm/preprocessor/parser/type_of_data.hpp"
#include <vm/preprocessor/validator/detail/stack_state.hpp>
#include <vm/preprocessor/parser/elements.hpp>
#include <sstream>

namespace vm::validator {
	namespace {
		/**
		 * @brief Performs static validation of the program.
		 *
		 * Validates:
		 * - Main function existence
		 * - Valid ret_tailcall signatures
		 * - No duplicate function declarations
		 * @todo Update that list when next checks are added
		 */
		class Validator {
		public:
			Validator(const parser::ParsedProgram& prog): program(prog) { log = dia::Logger(); }

			/**
			 * @brief Validates the program.
			 * Returns an empty optional in case of success and an error string on failure.
			 *
			 * @return base::Optional<std::string>
			 */
			base::Optional<std::string> validateProgram();

		private:
			const parser::ParsedProgram& program;
			dia::Logger                  log;

			void preprocessProgram();
			void validateMainExistance();
			void validateTailcallSignatures();
			void validateDuplicateFunctionDeclarations();
			void validateJumpStackStructure();
		};

		base::Optional<std::string> Validator::validateProgram() {
			validateMainExistance();
			validateTailcallSignatures();
			validateDuplicateFunctionDeclarations();
			validateJumpStackStructure();

			if (log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return stream.str();
			}
			return {};
		}

		void Validator::validateMainExistance() {
			bool main_found = false;
			for (auto& func: program.functions) {
				if (func->name.value.strView() == "main") {
					main_found = true;
					break;
				}
			}

			if (!main_found)
				log.log(makeBox<vm::validator::NoMainError>(*program.files_src_pos[0]));
		}

		void Validator::validateTailcallSignatures() {
			for (const auto& func: program.functions) {
				for (const auto& op: func->code->opcodes) {
					if (op->opcode_name.strView() == "ret_tailcall") {
						variant_match(op->args[0].arg) {
							variant_case(vm::opargs::FunctionName, function_name_arg) {
								auto maybe_called_func
									= program.name_to_func.atMaybe(function_name_arg.function_name);
								if_opt_some(maybe_called_func, called_func) {
									if (func->arg_size != called_func->arg_size) {
										log.log(makeBox<vm::validator::CallerCalledArgSizeMismatch>(
											*op->position
										));
									}
									if (func->local_size != called_func->local_size) {
										log.log(
											makeBox<vm::validator::CallerCalledStackSizeMismatch>(
												*op->position
											)
										);
									}
									if (func->ret_size != called_func->ret_size) {
										log.log(makeBox<vm::validator::CallerCalledRetSizeMismatch>(
											*op->position
										));
									}
								}
							}
							variant_default {
								CORE_ASSERT(
									false, "ret_tailcall should have a function name argument!"
								);
							}
						}
					}
				}
			}
		}

		void Validator::validateDuplicateFunctionDeclarations() { return; }

		void Validator::validateJumpStackStructure() {
			base::HashMap<base::StrID, int> parameter_count_for_funcion;
			for (const auto& type: program.types) {
				variant_match(type->datatype) {
					variant_case(parser::FunctionType, function_type) {
						parameter_count_for_funcion.put(
							function_type.name, function_type.parameters.size()
						);
					}
				}
			}
			for (const auto& func: program.functions) {
				int                             next_id = 0;
				std::vector<int>                id_stack;
				base::HashMap<base::StrID, int> top_id_at_label;
				if (!parameter_count_for_funcion.contains(func->name.value)) {
					// CORE_PANIC("No function type defined for a function");
					continue;
				}
				for (usize i = 0; i < parameter_count_for_funcion.at(func->name.value) + 2; i++) {
					id_stack.push_back(next_id);
					next_id++;
				}
				std::vector<std::pair<base::StrID, usize>> labels(
					func->code->label_position.begin(), func->code->label_position.end()
				);
				std::ranges::sort(labels, [](const auto& a, const auto& b) {
					return a.second > b.second;
				});
				for (usize i = 0; i < func->code->opcodes.size();) {
					auto& op = func->code->opcodes.at(i);
					if (!labels.empty()) {
						if (labels.back().second == i) {
							base::StrID label_name = labels.back().first;
							if (top_id_at_label.contains(label_name)) {
								if (top_id_at_label[label_name] != id_stack.back()) {
									log.log(makeBox<vm::validator::JumpStackStructureMismatch>(
										*op->position
									));
								}
							} else {
								top_id_at_label.put(label_name, id_stack.back());
							}
							labels.pop_back();
							continue;
						}
					}

					if (op->opcode_name.strView() == "init_type") {
						id_stack.push_back(next_id);
						next_id++;
					} else if (op->opcode_name.strView() == "deinit") {
						if (id_stack.size() >= 2)
							id_stack.pop_back();
						else
							log.log(makeBox<vm::validator::InvalidDeinit>(*op->position));
					} else if (op->opcode_name.strView() == "call_func") {
						variant_match(op->args[0].arg) {
							variant_case(opargs::FunctionName, function_name) {
								if (!parameter_count_for_funcion.contains(
										function_name.function_name
									)) {
									// CORE_PANIC("No function type defined for a function");
									continue;
								}
								for (usize j = 0;
								     j
								     < parameter_count_for_funcion.at(function_name.function_name);
								     j++) {
									if (id_stack.size() >= 2)
										id_stack.pop_back();
									else
										log.log(makeBox<vm::validator::InvalidDeinit>(*op->position)
										);
								}
							}
							variant_default {
								CORE_PANIC("expected function name after call_func opcode");
							}
						}
					} else if (op->opcode_name.strView() == "jmpRel_label"
					           || op->opcode_name.strView() == "jmpRelIf_label"
					           || op->opcode_name.strView() == "jmpRelNotIf_label") {
						variant_match(op->args[0].arg) {
							variant_case(opargs::Label, label) {
								base::StrID label_name = label.label_name;
								if (top_id_at_label.contains(label_name)) {
									if (top_id_at_label[label_name] != id_stack.back()) {
										log.log(makeBox<vm::validator::JumpStackStructureMismatch>(
											*op->position
										));
									}
								} else {
									top_id_at_label.put(label_name, id_stack.back());
								}
							}
							variant_default { CORE_PANIC("expected label name after jump opcode"); }
						}
					}
					i++;
				}
			}
		}
	}

	base::Optional<std::string> verify(const parser::ParsedProgram& program) {
		return Validator(program).validateProgram();
	}

}
