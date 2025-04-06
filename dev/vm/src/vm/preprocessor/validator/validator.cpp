#include "validator.hpp"

#include "errors.hpp"

#include <diagnostic/logger.hpp>

#include "base/optional.hpp"
#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/preprocessor/parser/elements.hpp>
#include <vm/preprocessor/validator/detail/stack_state.hpp>
#include <vm/program/opcode_args.hpp>

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
			void validateInheritanceHierarchy();
		};

		base::Optional<std::string> Validator::validateProgram() {
			validateMainExistance();
			validateTailcallSignatures();
			validateDuplicateFunctionDeclarations();
			validateInheritanceHierarchy();

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

		void Validator::validateInheritanceHierarchy() {
			for (auto& parser_type: program.types) {
				auto type = program.type_metadata
				                ->getTypeByName(VISIT(parser_type->datatype, dt, return dt.name))
				                .value();
				if_opt_some(type->getVTable(), vtable) {
					// Check implements/extends
					for (auto iface: vtable.implements)
						if (!iface->isInterface())
							log.log(makeBox<InvalidImplements>(*parser_type->position));
					variant_match(vtable.kind) {
						variant_case(VTable::Class, clazz) {
							auto is_extends_valid
								= clazz.extends
							          .map([](auto super) {
										  if_opt_some(super->getVTable(), super_vt) {
											  variant_match(super_vt.kind) {
												  variant_case(VTable::Class, super_clazz) {
													  if (super_clazz.modifier
											              != VTable::Class::Modifier::Final)
														  return true;
												  }
											  }
										  }
										  return false;
									  })
							          .value_or(true);  // Does not extend anything.
							if (!is_extends_valid)
								log.log(makeBox<InvalidExtends>(*parser_type->position));
						}
					}

					// Check if vtable exists
					// @TODO this is VERY temporary!!!
					// we need some sort of builtin vtable pointer type, not silly name checking
					auto has_vtable_pointer
						= type->getFieldType(0)
					          .map([](auto field_type) { return field_type->getName() == "VT"; })
					          .value_or(false);
					if (!has_vtable_pointer)
						log.log(makeBox<MissingVtablePointer>(*parser_type->position));

					// Check if interfaces are data-less
					if (std::holds_alternative<VTable::Interface>(vtable.kind)) {
						if (type->getFieldType(1).has_value())
							log.log(makeBox<InstanceDataInInterface>(*parser_type->position));
					}

					// Check if superclass fields are inherited
					variant_match(vtable.kind) {
						variant_case(VTable::Class, clazz) {
							if_opt_some(clazz.extends, super) {
								auto n_super_fields = super->getFieldCount().value();
								for (size_t i = 0; i < n_super_fields; i++)
									if (super->getFieldType(i) != type->getFieldType(i))
										log.log(makeBox<MissingAncestorField>(*parser_type->position
										));
							}
						}
					}
				}
			}
		}
	}

	base::Optional<std::string> verify(const parser::ParsedProgram& program) {
		return Validator(program).validateProgram();
	}
}
