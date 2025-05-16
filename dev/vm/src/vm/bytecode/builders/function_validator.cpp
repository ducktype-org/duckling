#include "function_validator.hpp"

#include <base/variant.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/type_of_data.hpp>

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
	using ExtensionTypes        = std::tuple<Op_ext_l64, Op_ext_type>;
	template<typename T>
	concept Extension = IsIn<T, ExtensionTypes>::VALUE;

	template<Extension E>
	struct ExtensionMetadata;

	template<>
	struct ExtensionMetadata<Op_ext_l64> {
		using RequiredAfter = std::tuple<>;
		using OptionalAfter = std::tuple<Op_load_l64_lptr_ofs, Op_store_lptr_l64_ofs>;
	};

	template<>
	struct ExtensionMetadata<Op_ext_type> {
		using RequiredAfter = std::tuple<Op_downcast_lptr_lptr>;
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

void FunctionValidator::validateInstruction(const Instruction& instruction) const {
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
		if (!local_name_to_type.contains(local.var_name)) throw UnknownLocalNameError(arg);      \
		CRef<TypeOfData> entry = local_name_to_type.at(local.var_name);                          \
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
				variant_match(*type) {
					variant_case_novalue(PointerType) {}
					variant_default { throw InvalidArgumentTypeError(arg); }
				}
			}

			STACK_LOCAL_CASE(8);
			STACK_LOCAL_CASE(16);
			STACK_LOCAL_CASE(32);
			STACK_LOCAL_CASE(64);
			variant_case(opargs::StackLocalPtr, local) {
				if (!local_name_to_type.contains(local.var_name)) throw UnknownLocalNameError(arg);
				CRef<TypeOfData> entry = local_name_to_type.at(local.var_name);
				variant_match(*entry) {
					variant_case(PointerType, pointer_type) { (void) pointer_type; }
					variant_default { throw InvalidArgumentTypeError(arg); }
				}
			}
			variant_case(opargs::StackLocalAny, local) {
				variant_match(instruction) {
					variant_case_novalue(Op_init_lany_type) {}
					// The default instruction for a label argument is a jump instruction.
					variant_default {
						if (!local_name_to_type.contains(local.var_name))
							throw UnknownLocalNameError(arg);
					}
				}
			}
			variant_case_novalue(opargs::Immediate) {}
			variant_case(opargs::Type, type_value) {
				if (!type_context.getTypes().contains(type_value.type_name))
					throw UnknownTypeError(arg);
			}
			variant_case(opargs::FunctionName, function_value) {}
			variant_case_novalue(opargs::BuiltinFunctionName) {}
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

			// All possible opargs must be handled. Unhandled opargs throw an exception.
			variant_default { CORE_PANIC("Unhandled argument case during validation\n"); }
		}
	}

	// We assert no cross-type operations on primitive types.
	if (arg_types.size() == 2) {
		if (arg_types.at(0).size == arg_types.at(1).size
		    && arg_types.at(0).name != arg_types.at(1).name)
			throw ArgumentMismatchError(instruction);
	}

	variant_match(instruction) {
		variant_case(Op_init_lany_type, instr) { validateArgInstantiable(instr.arg1); }
		variant_case(Op_alloc_lptr_type, instr) { validateArgInstantiable(instr.arg1); }
	}
}

void FunctionValidator::validateArgInstantiable(const opargs::Type& arg) const {
	auto type = type_context.getMetadata().at(arg.type_name);
	if (!type->isInstantiable()) throw UninstantiableValueError(arg);
}

void FunctionValidator::initStackState() {
	pushStackState(base::StrID("ret_val"), type.result);
	for (auto [idx, param]: std::views::enumerate(type.parameters))
		pushStackState(base::StrID(base::strConcat("arg", idx).c_str()), param);
}

void FunctionValidator::pushStackState(const opargs::StackLocalAny& local, const opargs::Type& type) {
	auto tod = type_context.getTypes().at(type.type_name);

	if (local_name_to_type.contains(local.var_name)) throw DuplicatedLocalNameError(local);

	stack_state.emplace_back(local.var_name, tod);
	local_name_to_type.put(local.var_name, tod);
}

void FunctionValidator::popStackState(const Op_deinit& cause) {
	if (stack_state.size() == 1) throw RetValDeinitError(cause);
	const auto& top = stack_state.back();
	local_name_to_type.erase(top.local_name);
	stack_state.pop_back();
}

void FunctionValidator::popCallArgs(const opargs::OpCodeFunctionArg& function) {
	auto fun_name = VISIT(function, f, return f.function_name);
	// Used for errors.
	auto generic_arg = VISIT(function, f, return opargs::OpCodeArg{ f });
	// TODO: remove atMaybe after checking.
	auto maybe_func_type
		= type_context.getTypes().atMaybe(fun_name).expect<UnknownFunctionError>(generic_arg);
	if (!std::holds_alternative<FunctionType>(*maybe_func_type))
		throw UnknownFunctionError(generic_arg);
	auto func_type     = std::get<FunctionType>(*maybe_func_type);
	bool check_ret_val = func_type.result != base::StrID("void");

	if (func_type.parameters.size() > stack_state.size() + check_ret_val)
		throw InvalidFunctionCallArgumentsError(generic_arg);
	for (auto param: func_type.parameters | std::views::reverse) {
		if (code::typeName(*stack_state.back().type) != param)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		stack_state.pop_back();
	}
	if (check_ret_val && code::typeName(*stack_state.back().type) != func_type.result)
		throw InvalidFunctionCallArgumentsError(generic_arg);
}

void FunctionValidator::validateTailcall(const opargs::OpCodeFunctionArg& function) const {
	auto fun_name = VISIT(function, f, return f.function_name);
	// Used for errors.
	auto generic_arg = VISIT(function, f, return opargs::OpCodeArg{ f });
	auto maybe_func_type
		= type_context.getTypes().atMaybe(fun_name).expect<UnknownFunctionError>(generic_arg);
	if (!std::holds_alternative<FunctionType>(*maybe_func_type))
		throw UnknownFunctionError(generic_arg);
	auto func_type = std::get<FunctionType>(*maybe_func_type);

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
	visited_instructions.resize(instructions.size());
	std::vector<std::tuple<usize, decltype(stack_state), decltype(local_name_to_type)>> dfs_stack{
		{ instructions.size(), {}, {} }  // sentinel
	};
	usize index = 0;

	while (index != instructions.size()) {
		validateExtension(index);
		// Validate non-control flow instruction there.
		validateInstruction(instructions[index]);

		visited_instructions[index] = true;

		variant_match(instructions[index]) {
			variant_case(Op_init_lany_type, instr) {
				pushStackState(instr.arg0, instr.arg1);
				index++;
			}
			variant_case(Op_deinit, instr) {
				popStackState(instr);
				index++;
			}
			variant_case(Op_label, instr) {
				match_optional(stack_at_label.atMaybe(instr.arg0.label_name)) {
					opt_some(label_state) {
						if (label_state != stack_state)
							throw StackStructureMismatchError(
								instr, jumps_to_label.at(instr.arg0.label_name)
							);
						std::tie(index, stack_state, local_name_to_type) = dfs_stack.back();
						dfs_stack.pop_back();
					}
					opt_none {
						stack_at_label.put(instr.arg0.label_name, stack_state);
						index++;
					}
				}
			}
			variant_case(Op_jmp_label, instr) { index = getLabelTarget(instr.arg0); }
			variant_case(Op_jmpIf_label, instr) {
				index++;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), stack_state, local_name_to_type);
			}
			variant_case(Op_jmpIfNot_label, instr) {
				index++;
				dfs_stack.emplace_back(getLabelTarget(instr.arg0), stack_state, local_name_to_type);
			}
			variant_case(Op_ret, instr) {
				std::tie(index, stack_state, local_name_to_type) = dfs_stack.back();
				dfs_stack.pop_back();
			}
			variant_case(Op_call_func, instr) {
				popCallArgs(instr.arg0);
				index++;
			}
			variant_case(Op_call_builtin_func, instr) {
				popCallArgs(instr.arg0);
				index++;
			}
			variant_case(Op_ret_tailcall_func, instr) {
				validateTailcall(instr.arg0);
				std::tie(index, stack_state, local_name_to_type) = dfs_stack.back();
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
	initStackState();
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
