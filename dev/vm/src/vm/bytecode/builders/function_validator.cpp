#include "function_validator.hpp"

#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/ref.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <variant>

using namespace vm::code::builders;

namespace {
	// Helpers for validation, mainly `ext_*` instructions
	using namespace vm::code;
	using namespace vm::code::instructions;

	template<typename T, typename Tup>
	struct IsIn;

	template<typename T, typename... Ts>
	struct IsIn<T, std::tuple<Ts...>> {
		static constexpr bool VALUE = (std::same_as<T, Ts> || ...);
	};

	template<typename... Tups>
	using Cat = decltype(std::tuple_cat(std::declval<Tups>()...));

	template<typename Tup>
	struct HoldsOneOfImpl;

	template<typename... Ts>
	struct HoldsOneOfImpl<std::tuple<Ts...>> {
		constexpr bool operator()(const Instruction& instr) {
			return (std::holds_alternative<Ts>(instr) || ...);
		}
	};

	template<typename Tup>
	constexpr bool holdsOneOf(const Instruction& instr) {
		return HoldsOneOfImpl<Tup>{}(instr);
	}

	using ValidLastInstructions = std::tuple<Op_ret, Op_ret_tailcall_func, Op_jmp_label>;
	using ExtensionTypes        = std::tuple<Op_ext_l64, Op_ext_type, Op_ext_field>;
	template<typename T>
	concept Extension = IsIn<T, ExtensionTypes>::VALUE;

	template<Extension E>
	struct ExtensionMetadata;

	template<>
	struct ExtensionMetadata<Op_ext_l64> {
		using RequiredAfter
			= std::tuple<Op_staticTableLoad_lany_lptr, Op_staticTableStore_lptr_lany>;
		using OptionalAfter = std::tuple<>;
	};

	template<>
	struct ExtensionMetadata<Op_ext_type> {
		using RequiredAfter = std::tuple<
			Op_downcast_lptr_lptr,
			Op_variantGetInner_lptr_lvnt,
			Op_variantGetInner_lptr_lptr>;
		using OptionalAfter = std::tuple<>;
	};

	template<>
	struct ExtensionMetadata<Op_ext_field> {
		using RequiredAfter
			= std::tuple<Op_structLea_lptr_lptr, Op_structLoad_lany_lptr, Op_structStore_lptr_lany>;
		using OptionalAfter = std::tuple<>;
	};

	template<typename Tup>
	struct CatRequired;

	template<typename... Ts>
	struct CatRequired<std::tuple<Ts...>> {
		using Value = Cat<typename ExtensionMetadata<Ts>::RequiredAfter...>;
	};

	template<Extension E>
	bool acceptsExtension(const Instruction& instr) {
		return holdsOneOf<
			Cat<typename ExtensionMetadata<E>::RequiredAfter,
		        typename ExtensionMetadata<E>::OptionalAfter>>(instr);
	}

	bool requiresSomeExtension(const Instruction& instr) {
		return holdsOneOf<CatRequired<ExtensionTypes>::Value>(instr);
	}
}

void FunctionValidator::validateExtension(usize instruction_index) const {
	// This check assumes that the last instruction in a function is non-extendable,
	// this is checked in `validate`.
	auto instruction     = instructions[instruction_index];
	auto predecessor     = instruction_index == 0 ? base::Optional<const Instruction&>{}
	                                              : instructions[instruction_index - 1];
	bool valid_extension = std::visit(
		[&]<typename T>(const T&) {
			if constexpr (Extension<T>)
				// If the current instruction is an extension, the situation is valid
			    // if the previous instruction can take this extension.
			    // Extensions must always come after some instruction, so it's invalid for it to be
			    // the first instruction in a function (to not have a predecessor).
				return predecessor.map(acceptsExtension<T>).valueOr(false);
			else
				// It is invalid if the current instruction is not an extension, but the previous
			    // instruction *requires* one. If there was no previous instruction, it's not invalid.
				return !predecessor.map(requiresSomeExtension).valueOr(false);
		},
		instruction
	);
	if (!valid_extension) throw InvalidInstructionExtensionError(instruction);
}

void FunctionValidator::validateArgTypes(
	const Instruction& instruction, const LocalStack& current_stack
) const {
	auto args = std::visit(
		[]<typename T>(T& instr) -> std::vector<opargs::OpCodeArg> {
			if constexpr (TwoArgumentOpcode<T>)
				return { instr.arg0, instr.arg1 };
			else if constexpr (OneArgumentOpcode<T>)
				return { instr.arg0 };
			else
				return {};
		},
		instruction
	);

	std::vector<PrimitiveType> arg_types;

	for (auto arg: args) {
		variant_match(arg) {
#define STACK_LOCAL_CASE(BIT_COUNT)                                                              \
	variant_case(opargs::StackLocalI##BIT_COUNT, local) {                                        \
		if (!current_stack.contains(local.var_name)) throw UnknownLocalNameError(arg);           \
		CRef<TypeOfData> entry = current_stack.at(local.var_name);                               \
		variant_match(*entry) {                                                                  \
			variant_case(PrimitiveType, primitive_type) {                                        \
				if (primitive_type.size != (BIT_COUNT / 8)) throw InvalidArgumentSizeError(arg); \
				arg_types.push_back(primitive_type);                                             \
			}                                                                                    \
			variant_default { throw InvalidArgumentTypeError(arg); }                             \
		}                                                                                        \
	}
#define GLOBAL_CASE(BIT_COUNT)                                                                   \
	variant_case(opargs::GlobalI##BIT_COUNT, global) {                                           \
		if (!globals.contains(global.global_data_name)) throw UnknownGlobalNameError(arg);       \
		CRef<GlobalData> entry = globals.at(global.global_data_name);                            \
		auto             type  = type_context.getTypes().at(entry->type);                        \
		variant_match(*type) {                                                                   \
			variant_case(PrimitiveType, primitive_type) {                                        \
				if (primitive_type.size != (BIT_COUNT / 8)) throw InvalidArgumentSizeError(arg); \
				arg_types.push_back(primitive_type);                                             \
			}                                                                                    \
			variant_default { throw InvalidArgumentTypeError(arg); }                             \
		}                                                                                        \
	}
			GLOBAL_CASE(8);
			GLOBAL_CASE(16);
			GLOBAL_CASE(32);
			GLOBAL_CASE(64);

			variant_case(opargs::GlobalPtr, global) {
				if (!globals.contains(global.global_data_name)) throw UnknownGlobalNameError(arg);
				CRef<GlobalData> entry = globals.at(global.global_data_name);
				auto             type  = type_context.getTypes().at(entry->type);
				if (!std::holds_alternative<PointerType>(*type))
					throw InvalidArgumentTypeError(arg);
			}

			STACK_LOCAL_CASE(8);
			STACK_LOCAL_CASE(16);
			STACK_LOCAL_CASE(32);
			STACK_LOCAL_CASE(64);
			variant_case(opargs::StackLocalPtr, local) {
				if (!current_stack.contains(local.var_name)) throw UnknownLocalNameError(arg);
				CRef<TypeOfData> type = current_stack.at(local.var_name);
				if (!std::holds_alternative<PointerType>(*type))
					throw InvalidArgumentTypeError(arg);
			}
			variant_case(opargs::StackLocalAny, local) {
				variant_match(instruction) {
					variant_case_novalue(Op_init_lany_type) {
						if (current_stack.contains(local.var_name))
							throw DuplicatedLocalNameError(arg);
					}
					variant_default {
						if (!current_stack.contains(local.var_name))
							throw UnknownLocalNameError(arg);
					}
				}
			}
			variant_case_novalue(opargs::Immediate) {}
			variant_case(opargs::Type, type_value) {
				if (!type_context.getTypes().contains(type_value.type_name))
					throw UnknownTypeError(arg);
			}
			variant_case(opargs::FunctionName, function_value) {
				auto fun_name    = function_value.function_name;
				auto generic_arg = opargs::OpCodeArg{ function_value };
				auto maybe_func_type
					= type_context.getTypes().atMaybe(fun_name).expect<UnknownFunctionError>(
						generic_arg
					);
				if (!std::holds_alternative<FunctionType>(*maybe_func_type))
					throw UnknownFunctionError(generic_arg);
			}
			variant_case(opargs::BuiltinFunctionName, function_value) {
				auto fun_name    = function_value.function_name;
				auto generic_arg = opargs::OpCodeArg{ function_value };
				auto maybe_func_type
					= type_context.getTypes().atMaybe(fun_name).expect<UnknownFunctionError>(
						generic_arg
					);
				if (!std::holds_alternative<FunctionType>(*maybe_func_type))
					throw UnknownFunctionError(generic_arg);
			}
			variant_case(opargs::Label, label_value) {
				variant_match(instruction) {
					variant_case_novalue(Op_label) {}
					// The default instruction for a label argument is a jump instruction.
					variant_default {
						if (!index_of_label.contains(label_value.label_name))
							throw UnknownLabelError(label_value);
					}
				}
			}

			variant_case(opargs::StackLocalVnt, variant) {
				if (!current_stack.contains(variant.var_name)) throw UnknownLocalNameError(arg);
				CRef<TypeOfData> type = current_stack.at(variant.var_name);
				if (!std::holds_alternative<VariantType>(*type))
					throw InvalidArgumentTypeError(arg);
			}

			variant_case(opargs::Field, field) {
				auto type = type_context.getTypes().at(field.type_name);
				variant_match(*type) {
					variant_case(DataType, ztruct) {
						bool good = false;
						for (auto& ztruct_field: ztruct.fields) {
							if (ztruct_field.name == field.field_name) {
								good = true;
								break;
							}
						}
						if (!good) throw UnknownFieldError(field);
					}
					variant_default { throw InvalidArgumentTypeError(arg); }
				}
			}

			// All possible opargs must be handled. Unhandled opargs panic.
			variant_default {
				CORE_PANIC("Unhandled argument case during validation");
				;
			}
		}
	}

	// We assert no cross-type operations on primitive types.
	if (arg_types.size() == 2) {
		if (arg_types.at(0).size == arg_types.at(1).size
		    && arg_types.at(0).name != arg_types.at(1).name)
			throw ArgumentMismatchError(instruction);
	}
}

void FunctionValidator::validateInstructionNonTrivially(
	const Instruction&                  instruction,
	[[maybe_unused]] const Instruction& next_instruction,
	const LocalStack&                   current_stack
) const {
	variant_match(instruction) {
		variant_case(Op_init_lany_type, instr) { validateArgInstantiable(instr.arg1); }
		variant_case(Op_alloc_lptr_type, instr) { validateArgInstantiable(instr.arg1); }
		variant_case(Op_upcast_lptr_lptr, instr) { validateUpcast(instr, current_stack); }

		variant_case_novalue(Comment) {}
		variant_case_novalue(Op_mov_l8_imm) {}
		variant_case_novalue(Op_mov_l8_l8) {}
		variant_case_novalue(Op_cmov_l8_l8) {}
		variant_case_novalue(Op_mov_l16_imm) {}
		variant_case_novalue(Op_mov_l16_l16) {}
		variant_case_novalue(Op_cmov_l16_l16) {}
		variant_case_novalue(Op_mov_l32_imm) {}
		variant_case_novalue(Op_mov_l32_l32) {}
		variant_case_novalue(Op_cmov_l32_l32) {}
		variant_case_novalue(Op_mov_l64_imm) {}
		variant_case_novalue(Op_mov_l64_l64) {}
		variant_case_novalue(Op_cmov_l64_l64) {}
		variant_case_novalue(Op_mov_l64_r0) {}
		variant_case_novalue(Op_mov_r0_l64) {}
		variant_case_novalue(Op_mov_g64_g64) {}
		variant_case_novalue(Op_mov_g64_l64) {}
		variant_case_novalue(Op_mov_g64_imm) {}
		variant_case_novalue(Op_mov_g32_g32) {}
		variant_case_novalue(Op_mov_g32_l32) {}
		variant_case_novalue(Op_mov_g32_imm) {}
		variant_case_novalue(Op_mov_g16_g16) {}
		variant_case_novalue(Op_mov_g16_l16) {}
		variant_case_novalue(Op_mov_g16_imm) {}
		variant_case_novalue(Op_mov_g8_g8) {}
		variant_case_novalue(Op_mov_g8_l8) {}
		variant_case_novalue(Op_mov_g8_imm) {}
		variant_case_novalue(Op_mov_gptr_lptr) {}
		variant_case_novalue(Op_mov_l64_g64) {}
		variant_case_novalue(Op_mov_l32_g32) {}
		variant_case_novalue(Op_mov_l16_g16) {}
		variant_case_novalue(Op_mov_l8_g8) {}
		variant_case_novalue(Op_mov_lptr_gptr) {}
		variant_case_novalue(Op_mov_lptr_lptr) {}
		variant_case_novalue(Op_add_l64_l64) {}
		variant_case_novalue(Op_add_l64_imm) {}
		variant_case_novalue(Op_add_l32_l32) {}
		variant_case_novalue(Op_add_l32_imm) {}
		variant_case_novalue(Op_sub_l64_l64) {}
		variant_case_novalue(Op_sub_l64_imm) {}
		variant_case_novalue(Op_sub_l32_l32) {}
		variant_case_novalue(Op_sub_l32_imm) {}
		variant_case_novalue(Op_mul_l64_l64) {}
		variant_case_novalue(Op_mul_l64_imm) {}
		variant_case_novalue(Op_mul_l32_l32) {}
		variant_case_novalue(Op_mul_l32_imm) {}
		variant_case_novalue(Op_mod_l64_l64) {}
		variant_case_novalue(Op_mod_l64_imm) {}
		variant_case_novalue(Op_mod_l32_l32) {}
		variant_case_novalue(Op_mod_l32_imm) {}
		variant_case_novalue(Op_div_l64_l64) {}
		variant_case_novalue(Op_div_l64_imm) {}
		variant_case_novalue(Op_div_l32_l32) {}
		variant_case_novalue(Op_div_l32_imm) {}
		variant_case_novalue(Op_neg_l64) {}
		variant_case_novalue(Op_neg_l32) {}
		variant_case_novalue(Op_cmpEq_l64_l64) {}
		variant_case_novalue(Op_cmpEq_l64_imm) {}
		variant_case_novalue(Op_cmpG_l64_l64) {}
		variant_case_novalue(Op_cmpG_l64_imm) {}
		variant_case_novalue(Op_cmpEq_l32_l32) {}
		variant_case_novalue(Op_cmpEq_l32_imm) {}
		variant_case_novalue(Op_cmpG_l32_l32) {}
		variant_case_novalue(Op_cmpG_l32_imm) {}
		variant_case_novalue(Op_cmpEq_l8_l8) {}
		variant_case_novalue(Op_cmpEq_l8_imm) {}
		variant_case_novalue(Op_cmpG_l8_l8) {}
		variant_case_novalue(Op_cmpG_l8_imm) {}
		variant_case_novalue(Op_cmpNull_lptr) {}
		variant_case_novalue(Op_variantSetInner_lvnt_type) {}
		variant_case_novalue(Op_variantGetInner_lptr_lvnt) {}
		variant_case_novalue(Op_variantSetInner_lptr_type) {}
		variant_case_novalue(Op_variantGetInner_lptr_lptr) {}
		variant_case_novalue(Op_label) {}
		variant_case_novalue(Op_jmp_label) {}
		variant_case_novalue(Op_jmpIf_label) {}
		variant_case_novalue(Op_jmpIfNot_label) {}
		variant_case_novalue(Op_call_func) {}
		variant_case_novalue(Op_call_builtin_func) {}
		variant_case_novalue(Op_ret_tailcall_func) {}
		variant_case_novalue(Op_ret) {}
		variant_case_novalue(Op_deinit) {}
		variant_case_novalue(Op_input_l64) {}
		variant_case_novalue(Op_output_l64) {}
		variant_case_novalue(Op_input_l32) {}
		variant_case_novalue(Op_output_l32) {}
		variant_case_novalue(Op_setVTable_lptr_type) {}
		variant_case_novalue(Op_downcast_lptr_lptr) {}
		variant_case_novalue(Op_free_lptr) {}
		variant_case_novalue(Op_store_lptr_lany) {}
		variant_case_novalue(Op_load_lany_lptr) {}
		variant_case_novalue(Op_ref_lptr_lany) {}
		variant_case_novalue(Op_structLea_lptr_lptr) {}
		variant_case_novalue(Op_structLoad_lany_lptr) {}
		variant_case_novalue(Op_structStore_lptr_lany) {}
		variant_case_novalue(Op_staticTableLea_lptr_lptr) {}
		variant_case_novalue(Op_staticTableLoad_lany_lptr) {}
		variant_case_novalue(Op_staticTableStore_lptr_lany) {}
		variant_case_novalue(Op_ext_l64) {}
		variant_case_novalue(Op_ext_type) {}
		variant_case_novalue(Op_ext_field) {}
		variant_case_novalue(Op_ext_type_field) {}
		variant_case_novalue(Op_ext_type_l64) {}
		variant_case_novalue(Op_nop) {}
		variant_case_novalue(Op_exit) {}
		variant_case_novalue(Op_breakpoint) {}

		variant_default { CORE_PANIC("Unhandled instruction: ", instructionToString(instruction)); }
	}
}

void FunctionValidator::validateArgInstantiable(const opargs::Type& arg) const {
	auto type = type_context.getMetadata().at(arg.type_name);
	if (!type->isInstantiable()) throw UninstantiableValueError(arg);
}

void FunctionValidator::validateUpcast(
	const Op_upcast_lptr_lptr& instruction, const LocalStack& current_stack
) const {
	auto dst_ptr_tod = current_stack.at(instruction.arg0.var_name);
	auto src_ptr_tod = current_stack.at(instruction.arg1.var_name);

	auto dst_type = type_context.getMetadata().at(getTypeKind<PointerType>(*dst_ptr_tod)->inner);
	auto src_type = type_context.getMetadata().at(getTypeKind<PointerType>(*src_ptr_tod)->inner);

	if (!src_type->inheritsFrom(dst_type)) throw InvalidUpcastError(instruction);
}

usize FunctionValidator::getLabelTarget(const opargs::Label& label) const {
	return index_of_label.at(label.label_name);
}

void FunctionValidator::preprocessLabels() {
	auto register_jump = [&](const auto& instr) {
		jumps_to_label.try_emplace(instr.arg0.label_name);
		jumps_to_label.at(instr.arg0.label_name).push_back(instr);
	};

	for (usize index = 0; index < instructions.size(); index++) {
		variant_match(instructions[index]) {
			variant_case(Op_label, instr) {
				auto [_, added] = index_of_label.insert_or_assign(instr.arg0.label_name, index);
				if (!added) throw DuplicatedLabelError(instr.arg0);
			}
			variant_case(Op_jmp_label, instr) { register_jump(instr); }
			variant_case(Op_jmpIf_label, instr) { register_jump(instr); }
			variant_case(Op_jmpIfNot_label, instr) { register_jump(instr); }
		}
	}
}

void FunctionValidator::traverseControlFlowGraph() {
	LocalStack local_stack(type, type_context);
	visited_instructions.resize(instructions.size());
	std::vector<std::tuple<usize, LocalStack>> dfs_stack{
		{ instructions.size(), local_stack }  // sentinel
	};
	usize index = 0;

	while (index != instructions.size()) {
		validateExtension(index);

		validateArgTypes(instructions[index], local_stack);

		if (index + 1 < instructions.size())
			validateInstructionNonTrivially(
				instructions[index], instructions[index + 1], local_stack
			);

		visited_instructions[index] = true;

		variant_match(instructions[index]) {
			variant_case(Op_init_lany_type, instr) {
				local_stack.push(instr.arg0, instr.arg1, type_context);
				index++;
			}
			variant_case(Op_deinit, instr) {
				local_stack.pop(instr);
				index++;
			}
			variant_case(Op_label, instr) {
				match_optional(stack_at_label.atMaybe(instr.arg0.label_name)) {
					opt_some(label_state) {
						if (label_state != local_stack.stack_state)
							throw StackStructureMismatchError(
								instr, jumps_to_label.at(instr.arg0.label_name)
							);
						std::tie(index, local_stack) = dfs_stack.back();
						dfs_stack.pop_back();
					}
					opt_none {
						stack_at_label.put(instr.arg0.label_name, local_stack.stack_state);
						index++;
					}
				}
			}
			variant_case(Op_jmp_label, instr) { index = getLabelTarget(instr.arg0); }
			variant_case(Op_jmpIf_label, instr) {
				index++;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), local_stack);
			}
			variant_case(Op_jmpIfNot_label, instr) {
				index++;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), local_stack);
			}
			variant_case(Op_ret, instr) {
				std::tie(index, local_stack) = dfs_stack.back();
				dfs_stack.pop_back();
			}
			variant_case(Op_call_func, instr) {
				local_stack.popCallArgs(instr.arg0, type_context);
				index++;
			}
			variant_case(Op_call_builtin_func, instr) {
				local_stack.popCallArgs(instr.arg0, type_context);
				index++;
			}
			variant_case(Op_ret_tailcall_func, instr) {
				local_stack.validateTailcall(instr.arg0, type_context, type);
				std::tie(index, local_stack) = dfs_stack.back();
				dfs_stack.pop_back();
			}
			variant_default { index++; }
		}
	}
}

void FunctionValidator::validateFunctionEnd() const {
	if (instructions.empty()
	    || (visited_instructions.back() && !holdsOneOf<ValidLastInstructions>(instructions.back())
	    )) {
		throw PathWithoutEndError(name.str);
	}
}

constexpr bool FunctionValidator::LocalStackEntry::operator==(const LocalStackEntry& other) const {
	return local_name == other.local_name && *type == *other.type;
}

FunctionValidator::FunctionValidator(
	Identifier                      name,
	const TypeContext&              type_context,
	const GlobalDataMap&            globals,
	const std::vector<Instruction>& instructions
):
	  name(name),
	  type_context(type_context),
	  globals(globals),
	  instructions(instructions),
	  type([&] {
		  auto maybe_func_type
			  = type_context.getTypes().atMaybe(name).expect<MissingFunctionalTypeError>(name);
		  if (!std::holds_alternative<FunctionType>(*maybe_func_type))
			  throw TypeIsNotFunctionalError(name);
		  return std::get<FunctionType>(*maybe_func_type);
	  }()) {}

void FunctionValidator::validate() {
	if (validated) CORE_PANIC("The validator can only run once.");
	preprocessLabels();
	traverseControlFlowGraph();
	validateFunctionEnd();
	validated = true;
}

std::vector<Instruction> FunctionValidator::extractReachableCode() {
	if (!validated) CORE_PANIC("Function has to be validated first.");
	std::vector<Instruction> out;
	for (auto [instruction, visited]: std::views::zip(instructions, visited_instructions))
		if (visited) out.push_back(instruction);
	return out;
}

FunctionValidator::LocalStack::LocalStack(const FunctionType& type, const TypeContext& type_context) {
	push(base::StrID("ret_val"), type.result, type_context);
	for (auto [idx, param]: std::views::enumerate(type.parameters))
		push(base::StrID(base::strConcat("arg", idx).c_str()), param, type_context);
}

void FunctionValidator::LocalStack::push(
	const opargs::StackLocalAny& local, const opargs::Type& type, const TypeContext& type_context
) {
	auto tod = type_context.getTypes().at(type.type_name);

	if (local_name_to_type.contains(local.var_name)) throw DuplicatedLocalNameError(local);

	stack_state.emplace_back(local.var_name, tod);
	local_name_to_type.put(local.var_name, tod);
}

void FunctionValidator::LocalStack::pop(const Op_deinit& cause) {
	if (stack_state.size() == 1) throw RetValDeinitError(cause);
	const auto& top = stack_state.back();
	local_name_to_type.erase(top.local_name);
	stack_state.pop_back();
}

bool FunctionValidator::LocalStack::contains(base::StrID local_name) const {
	return local_name_to_type.contains(local_name);
}

CRef<TypeOfData> FunctionValidator::LocalStack::at(base::StrID local_name) const {
	return local_name_to_type.at(local_name);
}

void FunctionValidator::LocalStack::popCallArgs(
	const opargs::OpCodeFunctionArg& function, const TypeContext& type_context
) {
	auto fun_name = VISIT(function, f, return f.function_name);
	// Used for errors.
	auto generic_arg   = VISIT(function, f, return opargs::OpCodeArg{ f });
	auto func_type     = std::get<FunctionType>(*type_context.getTypes().at(fun_name));
	bool check_ret_val = func_type.result != base::StrID("void");

	if (func_type.parameters.size() > stack_state.size() + check_ret_val)
		throw InvalidFunctionCallArgumentsError(generic_arg);
	for (auto param: func_type.parameters | std::views::reverse) {
		if (code::typeName(*stack_state.back().type) != param)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		local_name_to_type.erase(stack_state.back().local_name);
		stack_state.pop_back();
	}
	if (check_ret_val && code::typeName(*stack_state.back().type) != func_type.result)
		throw InvalidFunctionCallArgumentsError(generic_arg);
}

void FunctionValidator::LocalStack::validateTailcall(
	const opargs::OpCodeFunctionArg& function,
	const TypeContext&               type_context,
	const FunctionType&              type
) const {
	auto fun_name = VISIT(function, f, return f.function_name);
	// Used for errors.
	auto generic_arg = VISIT(function, f, return opargs::OpCodeArg{ f });
	auto func_type   = std::get<FunctionType>(*type_context.getTypes().at(fun_name));

	if (!(func_type.result == type.result && func_type.parameters == type.parameters))
		throw InvalidTailcallSignatureError(generic_arg);

	if (func_type.parameters.size() + 1 != stack_state.size())
		throw InvalidTailcallArgumentsError(generic_arg);

	if (code::typeName(*stack_state.front().type) != func_type.result)
		throw InvalidTailcallArgumentsError(generic_arg);
	for (auto [param, stack_elem]:
	     std::views::zip(func_type.parameters, stack_state | std::views::drop(1)))
		if (code::typeName(*stack_elem.type) != param)
			throw InvalidTailcallArgumentsError(generic_arg);
}
