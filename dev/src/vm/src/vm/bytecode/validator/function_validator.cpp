#include "function_validator.hpp"

#include "errors.hpp"

#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/ref.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <variant>

using namespace vm;
using namespace code;

namespace {
	// Helpers for validation, mainly `ext_*` instructions
	using namespace instructions;

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
	using DeinitializingInstructions
		= std::tuple<Op_deinit, Op_call_func, Op_call_builtin_func, Op_virtual_call_lptr_method>;
	using CallingInstructions = std::tuple<Op_call_func, Op_call_builtin_func>;
	template<typename T>
	concept Extension = IsIn<T, ExtensionTypes>::VALUE;
	template<typename T>
	concept DeinitializingInstruction = IsIn<T, DeinitializingInstructions>::VALUE;

	template<typename T>
	concept CallingInstruction = IsIn<T, CallingInstructions>::VALUE;

	template<Extension E>
	struct ExtensionMetadata;

	template<>
	struct ExtensionMetadata<Op_ext_l64> {
		using RequiredAfter
			= std::tuple<Op_fixedSizeTableLoad_lany_lptr, Op_fixedSizeTableStore_lptr_lany>;
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
	bool acceptsExtension(CRef<Instruction> instr) {
		return holdsOneOf<
			Cat<typename ExtensionMetadata<E>::RequiredAfter,
		        typename ExtensionMetadata<E>::OptionalAfter>>(*instr);
	}

	bool requiresSomeExtension(CRef<Instruction> instr) {
		return holdsOneOf<CatRequired<ExtensionTypes>::Value>(*instr);
	}

	template<class ExpectedT, class ErrorT = PointerTypeMismatchError, class... Args>
	const ExpectedT& expectPointerType(
		const PointerType&                    pointer,
		const StableObjIdNameMap<TypeOfData>& tod_map,
		Args&&... error_args
	) {
		const auto& pointed_type = tod_map.at(pointer.inner);
		if (!std::holds_alternative<ExpectedT>(*pointed_type))
			throw ErrorT(std::forward<Args>(error_args)...);
		return std::get<ExpectedT>(*pointed_type);
	}

	template<class ErrorT = PointerTypeMismatchError, class... Args>
	void validateStructExtFieldType(
		const DataType&      ztruct,
		const opargs::Field& field_arg,
		base::StrID          expected_field_type,
		Args&&... error_args
	) {
		if (ztruct.name != field_arg.type_name)
			throw StructTypeMismatchError(std::forward<Args>(error_args)...);

		base::StrID field_name = field_arg.field_name;
		Field       field      = *std::ranges::find(ztruct.fields, field_name, &Field::name);

		if (field.type != expected_field_type) throw ErrorT(std::forward<Args>(error_args)...);
	}
}

/**
 * @brief Represents a local stack variable.
 */
struct LocalStackEntry {
	base::StrID      local_name;
	CRef<TypeOfData> type;

	constexpr bool operator==(const LocalStackEntry& other) const {
		return local_name == other.local_name && *type == *other.type;
	}
};

class LocalStack {
	std::vector<LocalStackEntry> stack_state;

	// the following are CRefs instead of const& to allow copy/move.

	CRef<StableObjIdNameMap<TypeOfData>>         tod_map;
	[[maybe_unused]] CRef<TypeMetadata>          type_metadata;
	base::HashMap<base::StrID, CRef<TypeOfData>> local_name_to_type;

public:
	LocalStack(const LocalStack&)            = default;
	LocalStack(LocalStack&&)                 = default;
	LocalStack& operator=(const LocalStack&) = default;
	LocalStack& operator=(LocalStack&&)      = default;

	LocalStack(
		const FunctionType&                   function_type,
		const StableObjIdNameMap<TypeOfData>& tod_map,
		const TypeMetadata&                   type_metadata
	):
		  tod_map(&tod_map),
		  type_metadata(&type_metadata) {
		push(base::StrID("ret_val"), function_type.result);
		for (auto [idx, param]: std::views::enumerate(function_type.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param);
	}

	const std::vector<LocalStackEntry>& getStackState() const { return stack_state; }

	void push(const opargs::StackLocalAny& local, const opargs::Type& type) {
		auto tod = tod_map->at(type.type_name);

		if (local_name_to_type.contains(local.var_name)) throw DuplicatedLocalNameError(local);

		stack_state.emplace_back(local.var_name, tod);
		local_name_to_type.put(local.var_name, tod);
	}

	/**
	 * @brief Pops the top element from the stack state and updates local variable mappings.
	 * Can be only used with instructions which effectively deinitialize the local stack
	 * (deinit, call_func, virtual_call and call_builtin_func)
	 */
	template<DeinitializingInstruction InstructionType>
	void pop(const InstructionType& cause) {
		if (stack_state.size() == 1) throw RetValDeinitError(cause);
		const auto& top = stack_state.back();
		local_name_to_type.erase(top.local_name);
		stack_state.pop_back();
	}

	usize size() const { return stack_state.size(); }

	const LocalStackEntry& back() const { return stack_state.back(); }

	const LocalStackEntry& front() const { return stack_state.front(); }

	void castPrimitive(const opargs::OpCodePrimitiveArg& local, const opargs::Type& type) {
		auto  local_name = VISIT(local, l, return l.var_name);
		auto& curr_type  = local_name_to_type.at(local_name);
		auto  new_type   = tod_map->at(type.type_name);
		curr_type        = new_type;
		for (auto& entry: stack_state)
			if (entry.local_name == local_name) entry.type = new_type;
	}

	bool contains(base::StrID local_name) const { return local_name_to_type.contains(local_name); }

	CRef<TypeOfData> at(base::StrID local_name) const { return local_name_to_type.at(local_name); }
};

/**
 * @brief Class responsible for function validation.
 * Processes the control-flow graph and simulates
 * stack operations. Throws subclasses of ValidationError.
 */
class FunctionValidator {
	const StableObjIdNameMap<TypeOfData>& tod_map;
	const TypeMetadata&                   type_metadata;
	const StableObjIdNameMap<GlobalData>& globals;
	const Function&                       function;
	FunctionType                          function_type;

	std::vector<bool>                                        visited_instructions;
	base::HashMap<base::StrID, std::vector<LocalStackEntry>> stack_at_label;
	base::HashMap<base::StrID, usize>                        index_of_label;
	base::HashMap<base::StrID, std::vector<Instruction>>     jumps_to_label;

	template<CallingInstruction CallInstructionType>
	void validateCallAndPop(LocalStack& local_stack, const CallInstructionType& instr) {
		opargs::OpCodeFunctionArg func_arg = opargs::OpCodeFunctionArg{ instr.arg0 };
		auto                      fun_name = VISIT(func_arg, f, return f.function_name);
		// Used for errors.
		auto generic_arg   = VISIT(func_arg, f, return opargs::OpCodeArg{ f });
		auto func_type     = std::get<FunctionType>(*tod_map.at(fun_name));
		bool check_ret_val = func_type.result != base::StrID("void");

		if (func_type.parameters.size() > local_stack.size() + check_ret_val)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		for (auto param: func_type.parameters | std::views::reverse) {
			if (code::typeName(*local_stack.back().type) != param)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}
		if (check_ret_val && code::typeName(*local_stack.back().type) != func_type.result)
			throw InvalidFunctionCallArgumentsError(generic_arg);
	}

	/**
	 * @brief Validates the stack state at the moment of a method call.
	 *
	 * @note Method calls need their own handling.
	 * - The argument is only the name of the method and we need to find an implementation
	 *   corresponding to that name.
	 * - The first argument on the stack should be pointer which points to the same type as the
	 *   pointer passed as the `obj_ptr` (an argument to `virtual_call_lptr_method`).
	 *   Since virtual_method map contains only the signatures of methods, the implementations of
	 * them may declare a pointer to a different type (a pointer to a subclass). Validating just the
	 * pointer type name like in normal function calls would simply don't work.
	 */
	void validateMethodCallAndPop(LocalStack& local_stack, const Op_virtual_call_lptr_method& instr) {
		// @todo: This implementation seeking occurs in a couple of places. Think of a better way.
		// https://github.com/ducktype-org/rift-dev-zpp32/issues/55
		base::StrID impl_name;
		auto        it       = std::ranges::find_if(type_metadata, [&](const auto& type) {
            if_opt_some(
                type.getInheritanceMetadata(), inh_meta
            ) return inh_meta->virtual_methods.contains(instr.arg1.method_name);
            return false;
        });
		auto        inh_meta = it->getInheritanceMetadata().value();
		impl_name            = inh_meta->virtual_methods[instr.arg1.method_name]->getName();

		auto generic_arg   = opargs::OpCodeArg{ instr.arg1 };
		auto func_type     = std::get<FunctionType>(*tod_map.at(impl_name));
		bool check_ret_val = func_type.result != base::StrID("void");

		if (func_type.parameters.size() > local_stack.size() + check_ret_val)
			throw InvalidFunctionCallArgumentsError(generic_arg);

		for (auto param: func_type.parameters | std::views::drop(1) | std::views::reverse) {
			if (code::typeName(*local_stack.back().type) != param)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}

		auto type_name    = code::typeName(*local_stack.back().type);
		auto ptr_on_stack = std::get<PointerType>(*tod_map.at(type_name));
		auto ptr_in_call  = std::get<PointerType>(*local_stack.at(instr.arg0.var_name));
		if (ptr_in_call.inner != ptr_on_stack.inner)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		local_stack.pop(instr);

		if (check_ret_val && code::typeName(*local_stack.back().type) != func_type.result)
			throw InvalidFunctionCallArgumentsError(generic_arg);
	}

	void validateTailcall(
		const LocalStack&           local_stack,
		const Op_ret_tailcall_func& instr,
		const FunctionType&         current_function_type
	) const {
		opargs::OpCodeFunctionArg func_arg = opargs::OpCodeFunctionArg{ instr.arg0 };
		auto                      fun_name = VISIT(func_arg, f, return f.function_name);
		// Used for errors.
		auto generic_arg = VISIT(func_arg, f, return opargs::OpCodeArg{ f });
		auto func_type   = std::get<FunctionType>(*tod_map.at(fun_name));

		if (!(func_type.result == current_function_type.result
		      && func_type.parameters == current_function_type.parameters))
			throw InvalidTailcallSignatureError(generic_arg);

		if (func_type.parameters.size() + 1 != local_stack.size())
			throw InvalidTailcallArgumentsError(generic_arg);

		if (code::typeName(*local_stack.front().type) != func_type.result)
			throw InvalidTailcallArgumentsError(generic_arg);
		for (auto [param, stack_elem]: std::views::zip(
				 func_type.parameters, local_stack.getStackState() | std::views::drop(1)
			 ))
			if (code::typeName(*stack_elem.type) != param)
				throw InvalidTailcallArgumentsError(generic_arg);
	}

	/**
	 * @brief Validates instruction's arguments in a trivial, generic way, i.e. if an
	 * instruction expects a pointer argument then this function validates this argument really
	 * is a pointer, not a label or a primitive. In case of this function, an instruction can be
	 * thought of as an argument collection.
	 * @param instruction Instruction that is validated.
	 */
	void validateArgTypes(const Instruction& instruction, const LocalStack& current_stack) const {
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
		auto             type  = tod_map.at(entry->type);                                        \
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
					if (!globals.contains(global.global_data_name))
						throw UnknownGlobalNameError(arg);
					CRef<GlobalData> entry = globals.at(global.global_data_name);
					auto             type  = tod_map.at(entry->type);
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
					if (!tod_map.contains(type_value.type_name)) throw UnknownTypeError(arg);
				}
				variant_case(opargs::FunctionName, function_value) {
					auto fun_name    = function_value.function_name;
					auto generic_arg = opargs::OpCodeArg{ function_value };
					auto maybe_func_type
						= tod_map.atMaybe(fun_name).expect<UnknownFunctionError>(generic_arg);
					if (!std::holds_alternative<FunctionType>(*maybe_func_type))
						throw UnknownFunctionError(generic_arg);
				}
				variant_case(opargs::BuiltinFunctionName, function_value) {
					auto fun_name    = function_value.function_name;
					auto generic_arg = opargs::OpCodeArg{ function_value };
					auto maybe_func_type
						= tod_map.atMaybe(fun_name).expect<UnknownFunctionError>(generic_arg);
					if (!std::holds_alternative<FunctionType>(*maybe_func_type))
						throw UnknownFunctionError(generic_arg);
				}
				variant_case(opargs::MethodName, method_value) {
					auto method_name = method_value.method_name;
					auto generic_arg = opargs::OpCodeArg{ method_value };
					bool valid       = false;
					for (const auto& type: tod_map) {
						std::visit(
							[&](const auto& t) {
								if constexpr (requires { t.virtual_methods; }) {
									for (const auto& vmethod: t.virtual_methods)
										if (vmethod.name == method_name) valid = true;
								}
							},
							type
						);
						if (valid) break;
					}
					if (!valid) throw UnknownMethodError(generic_arg);
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
					auto type = tod_map.at(field.type_name);
					variant_match(*type) {
						variant_case(DataType, ztruct) {
							if (std::ranges::find(ztruct.fields, field.field_name, &Field::name)
							    == ztruct.fields.end())
								throw UnknownFieldError(field);
						}
						variant_default { throw InvalidArgumentTypeError(arg); }
					}
				}

				// All possible opargs must be handled. Unhandled opargs panic.
				variant_default {
					CORE_PANIC("Unhandled argument case during validation: ", argumentToString(arg));
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

	/**
	 * @brief Validates instruction's arguments non-trivially - using specific logic for each
	 * instruction. For instance, an instruction may expect type `T` as arg0, a `Pointer<T>` as
	 * arg1 and another `Pointer<T>` as an extension. This is the place to express such logic.
	 * @param instruction Instruction that is validated.
	 * @param next_instruction Optional next instruction. Used when expecting e.g. `ext_*`.
	 * @note Presence of extensions is checked by different function: `validateExtension`.
	 */
	void validateArgTypesNonTrivially(
		const Instruction&                                       instruction,
		[[maybe_unused]] base::Optional<base::CRef<Instruction>> next_instruction,
		const LocalStack&                                        current_stack
	) const {
		variant_match(instruction) {
			variant_case(Op_init_lany_type, instr) { validateArgInstantiable(instr.arg1); }
			variant_case(Op_alloc_lptr_type, instr) {
				validateArgInstantiable(instr.arg1);
				CRef<TypeOfData> variable = current_stack.at(instr.arg0.var_name);
				PointerType      pointer  = std::get<PointerType>(*variable);
				if (pointer.inner != instr.arg1.type_name) throw PointerTypeMismatchError(instr);
			}
			variant_case(Op_upcast_lptr_lptr, instr) { validateUpcast(instr, current_stack); }
			variant_case(Op_cast_l8_type, instr) {
				validatePrimitiveCast(instr.arg0, instr.arg1, instruction, current_stack);
			}
			variant_case(Op_cast_l16_type, instr) {
				validatePrimitiveCast(instr.arg0, instr.arg1, instruction, current_stack);
			}
			variant_case(Op_cast_l32_type, instr) {
				validatePrimitiveCast(instr.arg0, instr.arg1, instruction, current_stack);
			}
			variant_case(Op_cast_l64_type, instr) {
				validatePrimitiveCast(instr.arg0, instr.arg1, instruction, current_stack);
			}

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
			
			variant_case_novalue(Op_fadd_l64_l64) {}
			variant_case_novalue(Op_fadd_l64_imm) {}
			variant_case_novalue(Op_fadd_l32_l32) {}
			variant_case_novalue(Op_fadd_l32_imm) {}

			variant_case_novalue(Op_fsub_l64_l64) {}
			variant_case_novalue(Op_fsub_l64_imm) {}
			variant_case_novalue(Op_fsub_l32_l32) {}
			variant_case_novalue(Op_fsub_l32_imm) {}

			variant_case_novalue(Op_fmul_l64_l64) {}
			variant_case_novalue(Op_fmul_l64_imm) {}
			variant_case_novalue(Op_fmul_l32_l32) {}
			variant_case_novalue(Op_fmul_l32_imm) {}

			variant_case_novalue(Op_fdiv_l64_l64) {}
			variant_case_novalue(Op_fdiv_l64_imm) {}
			variant_case_novalue(Op_fdiv_l32_l32) {}
			variant_case_novalue(Op_fdiv_l32_imm) {}

			variant_case_novalue(Op_fneg_l64) {}
			variant_case_novalue(Op_fneg_l32) {}
			variant_case(Op_variantSetInner_lvnt_type, instr) {
				const auto& variant_type
					= std::get<VariantType>(*current_stack.at(instr.arg0.var_name));
				base::StrID                     wanted_type    = instr.arg1.type_name;
				const std::vector<base::StrID>& possible_types = variant_type.variant_alternatives;
				if (!std::ranges::contains(possible_types, wanted_type))
					throw VariantTypeMismatchError(instr);
			}
			variant_case(Op_variantGetInner_lptr_lvnt, instr) {
				const auto& variant_type
					= std::get<VariantType>(*current_stack.at(instr.arg1.var_name));
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				base::StrID                     wanted_type    = pointer_type.inner;
				const std::vector<base::StrID>& possible_types = variant_type.variant_alternatives;
				if (!std::ranges::contains(possible_types, wanted_type))
					throw VariantTypeMismatchError(instr);

				const auto& ext = std::get<Op_ext_type>(*next_instruction.value());
				if (ext.arg0.type_name != wanted_type) throw VariantTypeMismatchError(instr);
			}
			variant_case(Op_variantSetInner_lptr_type, instr) {
				const auto& variant_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));

				const auto& variant_type
					= expectPointerType<VariantType>(variant_pointer, tod_map, instr);

				base::StrID wanted_type = instr.arg1.type_name;
				if (!std::ranges::contains(variant_type.variant_alternatives, wanted_type))
					throw VariantTypeMismatchError(instr);
			}
			variant_case(Op_variantGetInner_lptr_lptr, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				base::StrID wanted_type = pointer_type.inner;

				const auto& variant_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg1.var_name));
				const auto& variant_type
					= expectPointerType<VariantType>(variant_pointer, tod_map, instr);

				if (!std::ranges::contains(variant_type.variant_alternatives, wanted_type))
					throw VariantTypeMismatchError(instr);

				const auto& ext = std::get<Op_ext_type>(*next_instruction.value());
				if (ext.arg0.type_name != wanted_type) throw VariantTypeMismatchError(instr);
			}
			variant_case_novalue(Op_label) {}
			variant_case_novalue(Op_jmp_label) {}
			variant_case_novalue(Op_jmpIf_label) {}
			variant_case_novalue(Op_jmpIfNot_label) {}
			variant_case_novalue(Op_call_func) {}
			variant_case_novalue(Op_call_builtin_func) {}
			variant_case(Op_virtual_call_lptr_method, instr) {
				// For a method all to be valid, the called method has to be declared as a virtual
				// method in this inheritable or it's superclasses or interfaces.
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				const auto& inh_meta
					= type_metadata.at(pointer_type.inner)->getInheritanceMetadata();

				const auto& obj_type = type_metadata.at(pointer_type.inner);

				bool                          valid                  = false;
				std::function<void(TypeCRef)> check_for_superclasses = [&](TypeCRef inh_type) {
					if (valid) return;
					if_opt_some(inh_type->getInheritanceMetadata(), imd) {
						if (imd->virtual_methods.contains(instr.arg1.method_name)) {
							valid = true;
							return;
						}
						for (const auto& iface: imd->implements) check_for_superclasses(iface);

						if_opt_some(inh_type->getSuperClass(), super) {
							check_for_superclasses(super);
						}
					}
				};
				check_for_superclasses(obj_type);
				if (!inh_meta.has_value() || !valid) throw InvalidVirtualCallError(instr);
			}
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
			variant_case(Op_store_lptr_lany, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				CRef<TypeOfData> other_type      = current_stack.at(instr.arg1.var_name);
				base::StrID      other_type_name = typeName(*other_type);
				if (pointer_type.inner != other_type_name) throw PointerTypeMismatchError(instr);
			}
			variant_case(Op_load_lany_lptr, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.arg1.var_name));
				CRef<TypeOfData> other_type      = current_stack.at(instr.arg0.var_name);
				base::StrID      other_type_name = typeName(*other_type);
				if (pointer_type.inner != other_type_name) throw PointerTypeMismatchError(instr);
			}
			variant_case(Op_ref_lptr_lany, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				CRef<TypeOfData> other_type      = current_stack.at(instr.arg1.var_name);
				base::StrID      other_type_name = typeName(*other_type);
				if (pointer_type.inner != other_type_name) throw PointerTypeMismatchError(instr);
			}
			variant_case(Op_structLea_lptr_lptr, instr) {
				const auto& destination
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));

				const auto& ztruct_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg1.var_name));
				const auto& ztruct = expectPointerType<DataType>(ztruct_pointer, tod_map, instr);

				Op_ext_field field_instr = std::get<Op_ext_field>(*next_instruction.value());
				validateStructExtFieldType(ztruct, field_instr.arg0, destination.inner, instr);
			}
			variant_case(Op_structLoad_lany_lptr, instr) {
				const auto& destination = current_stack.at(instr.arg0.var_name);

				const auto& ztruct_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg1.var_name));
				const auto& ztruct = expectPointerType<DataType>(ztruct_pointer, tod_map, instr);

				Op_ext_field field_instr = std::get<Op_ext_field>(*next_instruction.value());
				validateStructExtFieldType(ztruct, field_instr.arg0, typeName(*destination), instr);
			}
			variant_case(Op_structStore_lptr_lany, instr) {
				const auto& source = current_stack.at(instr.arg1.var_name);

				const auto& ztruct_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				const auto& ztruct = expectPointerType<DataType>(ztruct_pointer, tod_map, instr);

				Op_ext_field field_instr = std::get<Op_ext_field>(*next_instruction.value());
				validateStructExtFieldType(ztruct, field_instr.arg0, typeName(*source), instr);
			}
			variant_case(Op_fixedSizeTableLea_lptr_lptr, instr) {
				const auto& destination
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg1.var_name));
				const auto& table_type
					= expectPointerType<FixedSizeTableType>(table_pointer, tod_map, instr);

				if (destination.inner != table_type.inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			variant_case(Op_fixedSizeTableLoad_lany_lptr, instr) {
				const auto& destination = current_stack.at(instr.arg0.var_name);

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg1.var_name));
				const auto& table_type
					= expectPointerType<FixedSizeTableType>(table_pointer, tod_map, instr);

				if (typeName(*destination) != table_type.inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			variant_case(Op_fixedSizeTableStore_lptr_lany, instr) {
				const auto& source = current_stack.at(instr.arg1.var_name);

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.arg0.var_name));
				const auto& table_type
					= expectPointerType<FixedSizeTableType>(table_pointer, tod_map, instr);

				if (table_type.inner != typeName(*source))
					throw FixedSizeTableTypeMismatchError(instr);
			}
			variant_case_novalue(Op_ext_l64) {}
			variant_case_novalue(Op_ext_type) {}
			variant_case_novalue(Op_ext_field) {}
			variant_case_novalue(Op_ext_type_field) {}
			variant_case_novalue(Op_ext_type_l64) {}
			variant_case_novalue(Op_nop) {}
			variant_case_novalue(Op_exit) {}
			variant_case_novalue(Op_breakpoint) {}

			variant_default {
				CORE_PANIC("Unhandled instruction: ", instructionToString(instruction));
			}
		}
	}

	/**
	 * @brief Meant to be called for every instruction in a function, not just extension.
	 * In case it's an extension, it validates whether `predecessor` really expected this
	 * extension.
	 */
	void validateExtension(
		base::Optional<base::CRef<Instruction>> predecessor, const Instruction& instruction
	) const {
		// This check assumes that the last instruction in a function is non-extendable,
		// this is checked in `validate`.
		bool valid_extension = std::visit(
			[&]<typename T>(const T&) {
				if constexpr (Extension<T>)
					// If the current instruction is an extension, the situation is valid
				    // if the previous instruction can take this extension.
				    // Extensions must always come after some instruction, so it's invalid for it to
				    // be the first instruction in a function (to not have a predecessor).
					return predecessor.map(acceptsExtension<T>).copyValueOr(false);
				else
					// It is invalid if the current instruction is not an extension, but the
				    // previous instruction *requires* one. If there was no previous instruction,
				    // it's not invalid.
					return !predecessor.map(requiresSomeExtension).copyValueOr(false);
			},
			instruction
		);
		if (!valid_extension) throw InvalidInstructionExtensionError(instruction);
	}

	/**
	 * @brief Validates, whether given type is really instantiable, e.g. it's a primitive, or a
	 * real data, not an abstract class or an interface.
	 */
	void validateArgInstantiable(const opargs::Type& arg) const {
		auto type = type_metadata.at(arg.type_name);
		if (!type->isInstantiable()) throw UninstantiableValueError(arg);
	}

	void validateUpcast(const Op_upcast_lptr_lptr& instruction, const LocalStack& current_stack)
		const {
		auto dst_ptr_tod = current_stack.at(instruction.arg0.var_name);
		auto src_ptr_tod = current_stack.at(instruction.arg1.var_name);

		auto dst_type = type_metadata.at(getTypeKind<PointerType>(*dst_ptr_tod)->inner);
		auto src_type = type_metadata.at(getTypeKind<PointerType>(*src_ptr_tod)->inner);

		if (!src_type->inheritsFrom(dst_type)) throw InvalidUpcastError(instruction);
	}

	void validatePrimitiveCast(
		const opargs::OpCodePrimitiveArg& local,
		const opargs::Type&               type,
		const Instruction&                instruction,
		const LocalStack&                 current_stack
	) const {
		// These are guaranteed to exist by `validateArgTypes`.
		auto curr_type      = current_stack.at(VISIT(local, l, return l.var_name));
		auto new_type       = tod_map.at(type.type_name);
		auto curr_primitive = getTypeKind<PrimitiveType>(*curr_type).value();

		auto new_primitive
			= getTypeKind<PrimitiveType>(*new_type).expect<NonPrimitiveCastError>(type);
		if (curr_primitive.size != new_primitive.size) throw CastSizeMismatchError(instruction);
	}

	void validateFunctionEnd() const {
		if (function.body.empty()
		    || (visited_instructions.back()
		        && !holdsOneOf<ValidLastInstructions>(function.body.back()))) {
			throw PathWithoutEndError(function.name);
		}
	}

	usize getLabelTarget(const opargs::Label& label) const {
		return index_of_label.at(label.label_name);
	}

	void preprocessLabels() {
		auto register_jump = [&](const auto& instr) {
			jumps_to_label.try_emplace(instr.arg0.label_name);
			jumps_to_label.at(instr.arg0.label_name).push_back(instr);
		};

		for (usize index = 0; index < function.body.size(); index++) {
			variant_match(function.body[index]) {
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

	void traverseControlFlowGraph() {
		LocalStack local_stack(function_type, tod_map, type_metadata);
		visited_instructions.resize(function.body.size());
		std::vector<std::tuple<usize, LocalStack>> dfs_stack{
			{ function.body.size(), local_stack }  // sentinel
		};
		usize index = 0;

		auto& instructions = function.body;

		while (index != function.body.size()) {
			validateExtension(
				index > 0 ? &instructions[index - 1] : base::Optional<base::CRef<Instruction>>(),
				instructions[index]
			);

			validateArgTypes(instructions[index], local_stack);

			validateArgTypesNonTrivially(
				instructions[index],
				index + 1 < instructions.size() ? &instructions[index + 1]
												: base::Optional<base::CRef<Instruction>>(),
				local_stack
			);

			visited_instructions[index] = true;

			variant_match(instructions[index]) {
				variant_case(Op_init_lany_type, instr) {
					local_stack.push(instr.arg0, instr.arg1);
					index++;
				}
				variant_case(Op_deinit, instr) {
					local_stack.pop(instr);
					index++;
				}
				variant_case(Op_label, instr) {
					match_optional(stack_at_label.atMaybe(instr.arg0.label_name)) {
						opt_some(label_state) {
							if (*label_state != local_stack.getStackState())
								throw StackStructureMismatchError(
									instr, jumps_to_label.at(instr.arg0.label_name)
								);
							std::tie(index, local_stack) = dfs_stack.back();
							dfs_stack.pop_back();
						}
						opt_none {
							stack_at_label.put(instr.arg0.label_name, local_stack.getStackState());
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
					validateCallAndPop(local_stack, instr);
					index++;
				}
				variant_case(Op_call_builtin_func, instr) {
					validateCallAndPop(local_stack, instr);
					index++;
				}
				variant_case(Op_virtual_call_lptr_method, instr) {
					validateMethodCallAndPop(local_stack, instr);
					index++;
				}
				variant_case(Op_ret_tailcall_func, instr) {
					validateTailcall(local_stack, instr, function_type);
					std::tie(index, local_stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
#define HANDLE_CAST(SIZE)                                  \
	variant_case(Op_cast_l##SIZE##_type, instr) {          \
		local_stack.castPrimitive(instr.arg0, instr.arg1); \
		index++;                                           \
	}

				FOR_EACH(HANDLE_CAST, 8, 16, 32, 64)
#undef HANDLE_CAST
				variant_default { index++; }
			}
		}
	}

public:
	FunctionValidator(
		const StableObjIdNameMap<TypeOfData>& tod_map,
		const TypeMetadata&                   type_metadata,
		const StableObjIdNameMap<GlobalData>& globals,
		const Function&                       function
	):
		  tod_map(tod_map),
		  type_metadata(type_metadata),
		  globals(globals),
		  function(function),
		  function_type([&] {
			  auto maybe_func_type
				  = tod_map.atMaybe(function.name).expect<MissingFunctionalTypeError>(function.name);
			  if (!std::holds_alternative<FunctionType>(*maybe_func_type))
				  throw TypeIsNotFunctionalError(function.name);
			  return std::get<FunctionType>(*maybe_func_type);
		  }()) {}

	std::vector<Instruction> validateAndExtractReachableCode() {
		preprocessLabels();
		traverseControlFlowGraph();
		validateFunctionEnd();

		std::vector<Instruction> out;
		for (auto [instruction, visited]: std::views::zip(function.body, visited_instructions))
			if (visited) out.push_back(instruction);
		return out;
	}
};

vm::code::Function vm::code::validateAndExtractReachableCode(
	const StableObjIdNameMap<TypeOfData>& tod_map,
	const TypeMetadata&                   type_metadata,
	const StableObjIdNameMap<GlobalData>& globals_map,
	const Function&                       function
) {
	FunctionValidator validator(tod_map, type_metadata, globals_map, function);

	Function new_function;
	new_function.name         = function.name;
	new_function.body         = validator.validateAndExtractReachableCode();
	new_function.bytecode_pos = function.bytecode_pos;

	return new_function;
}
