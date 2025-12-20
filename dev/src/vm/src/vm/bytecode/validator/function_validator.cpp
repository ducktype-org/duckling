#include "function_validator.hpp"

#include "errors.hpp"

#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <variant>

using namespace vm;
using namespace code;

namespace {
	// Helpers
	using namespace instructions;

	constexpr std::array VALID_LAST_OPCODES
		= { OpCode::Op_ret, OpCode::Op_ret_tailcall_func, OpCode::Op_jmp_label };

	using DeinitializingInstructions = std::tuple<
		Op_deinit,
		Op_call_func,
		Op_call_builtinfunc,
		Op_call_cfunc,
		Op_virtual_call_lptr_method>;
	using CallingInstructions = std::tuple<Op_call_func, Op_call_builtinfunc, Op_call_cfunc>;

	template<typename T>
	concept DeinitializingInstruction = base::IsTupleMember<T, DeinitializingInstructions>;

	template<typename T>
	concept CallingInstruction = base::IsTupleMember<T, CallingInstructions>;

	template<class ExpectedT, class ErrorT = PointerTypeMismatchError, class... Args>
	const ExpectedT& expectPointerType(
		const PointerType& pointer, const ObjIdNameMap<TypeOfData>& tod_map, Args&&... error_args
	) {
		const auto& pointed_type = tod_map.at(pointer.inner);
		if (!std::holds_alternative<ExpectedT>(*pointed_type))
			throw ErrorT(std::forward<Args>(error_args)...);
		return std::get<ExpectedT>(*pointed_type);
	}

	template<class ErrorT = PointerTypeMismatchError, class... Args>
	void validateStructFieldType(
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

	CRef<ObjIdNameMap<TypeOfData>>               tod_map;
	[[maybe_unused]] CRef<TypeMetadata>          type_metadata;
	base::HashMap<base::StrID, CRef<TypeOfData>> local_name_to_type;

public:
	LocalStack(const LocalStack&)            = default;
	LocalStack(LocalStack&&)                 = default;
	LocalStack& operator=(const LocalStack&) = default;
	LocalStack& operator=(LocalStack&&)      = default;

	LocalStack(
		const FuncSignature&            signature,
		const ObjIdNameMap<TypeOfData>& tod_map,
		const TypeMetadata&             type_metadata
	):
		  tod_map(&tod_map),
		  type_metadata(&type_metadata) {
		push(base::StrID("ret_val"), signature.result_type.str);
		for (auto [idx, param]: std::views::enumerate(signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param.str);
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
	 * (deinit, call_func, virtual_call and call_builtinfunc)
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
	const ObjIdNameMap<TypeOfData>&                  tod_map;
	const TypeMetadata&                              type_metadata;
	const ObjIdNameMap<GlobalData>&                  globals;
	const base::HashMap<base::StrID, FuncSignature>& signatures;
	const ObjIdNameMap<ExternalCFunction>&           ext_c_signatures;
	const Function&                                  function;

	std::vector<bool>                                        visited_instructions;
	base::HashMap<base::StrID, std::vector<LocalStackEntry>> stack_at_label;
	base::HashMap<base::StrID, usize>                        index_of_label;
	base::HashMap<base::StrID, std::vector<Instruction>>     jumps_to_label;

	template<CallingInstruction CallInstructionType>
	void validateCallAndPop(LocalStack& local_stack, const CallInstructionType& instr) {
		// Used for errors.
		auto                generic_arg = opargs::OpCodeArg{ instr.function };
		CRef<FuncSignature> signature   = [&] -> CRef<FuncSignature> {
            if constexpr (std::is_same_v<opargs::BuiltinFunctionName, decltype(instr.function)>)
                return *builtins::getBuiltinFunctionSignature(instr.function.function_name);
            if constexpr (std::is_same_v<opargs::ExtCFunctionName, decltype(instr.function)>)
                return &ext_c_signatures.at(instr.function.function_name)->signature;
            return &signatures.at(instr.function.function_name);
		}();

		bool check_ret_val = signature->result_type.str != base::StrID("void");

		if (signature->parameters.size() > local_stack.size() + check_ret_val)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		for (auto param: signature->parameters | std::views::reverse) {
			if (typeName(*local_stack.back().type) != param.str)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}
		if (check_ret_val && typeName(*local_stack.back().type) != signature->result_type.str)
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
		// @TODO: #962 This implementation seeking occurs in a couple of places. Think of a better way.
		base::StrID impl_name;
		auto        it       = std::ranges::find_if(type_metadata, [&](const auto& type) {
            if_opt_some(
                type.getInheritanceMetadata(), inh_meta
            ) return inh_meta->vtable.contains(instr.method.method_name);
            return false;
        });
		auto        inh_meta = it->getInheritanceMetadata().value();
		impl_name            = inh_meta->vtable[instr.method.method_name];

		auto generic_arg   = opargs::OpCodeArg{ instr.method };
		auto signature     = signatures.at(impl_name);
		bool check_ret_val = signature.result_type.str != base::StrID("void");

		if (signature.parameters.size() > local_stack.size() + check_ret_val)
			throw InvalidFunctionCallArgumentsError(generic_arg);

		for (auto param: signature.parameters | std::views::drop(1) | std::views::reverse) {
			if (code::typeName(*local_stack.back().type) != param.str)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}

		auto type_name    = code::typeName(*local_stack.back().type);
		auto ptr_on_stack = std::get<PointerType>(*tod_map.at(type_name));
		auto ptr_in_call  = std::get<PointerType>(*local_stack.at(instr.object_ptr.var_name));
		if (ptr_in_call.inner != ptr_on_stack.inner)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		local_stack.pop(instr);

		if (check_ret_val && code::typeName(*local_stack.back().type) != signature.result_type.str)
			throw InvalidFunctionCallArgumentsError(generic_arg);
	}

	void validateTailcall(
		const LocalStack&           local_stack,
		const Op_ret_tailcall_func& instr,
		const FuncSignature&        current_signature
	) const {
		opargs::OpCodeFunctionArg func_arg = opargs::OpCodeFunctionArg{ instr.function };
		auto                      fun_name = VISIT(func_arg, f, return f.function_name);
		// Used for errors.
		auto generic_arg = VISIT(func_arg, f, return opargs::OpCodeArg{ f });
		auto signature   = signatures.at(fun_name);

		if (!(signature.result_type.str == current_signature.result_type.str
		      && signature.parameters == current_signature.parameters))
			throw InvalidTailcallSignatureError(generic_arg);

		if (signature.parameters.size() + 1 != local_stack.size())
			throw InvalidTailcallArgumentsError(generic_arg);

		if (code::typeName(*local_stack.front().type) != signature.result_type.str)
			throw InvalidTailcallArgumentsError(generic_arg);
		for (auto [param, stack_elem]: std::views::zip(
				 signature.parameters, local_stack.getStackState() | std::views::drop(1)
			 ))
			if (code::typeName(*stack_elem.type) != param.str)
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
		std::vector<PrimitiveType> primitive_args;

		for (auto arg: instruction.args()) {
			variant_match(arg) {
#define STACK_LOCAL_CASE(BIT_COUNT)                                                        \
	variant_case(CRef<opargs::StackLocal##BIT_COUNT>, local) {                             \
		if (!current_stack.contains(local->var_name)) throw UnknownLocalNameError(*local); \
		CRef<TypeOfData> entry = current_stack.at(local->var_name);                        \
		variant_match(*entry) {                                                            \
			variant_case(PrimitiveType, primitive_type) {                                  \
				if (primitive_type.size != (BIT_COUNT / 8))                                \
					throw InvalidArgumentSizeError(*local);                                \
				primitive_args.push_back(primitive_type);                                  \
			}                                                                              \
			variant_default { throw InvalidArgumentTypeError(*local); }                    \
		}                                                                                  \
	}
#define GLOBAL_CASE(BIT_COUNT)                                                                  \
	variant_case(CRef<opargs::Global##BIT_COUNT>, global) {                                     \
		if (!globals.contains(global->global_data_name)) throw UnknownGlobalNameError(*global); \
		CRef<GlobalData> entry = globals.at(global->global_data_name);                          \
		auto             type  = tod_map.at(entry->type);                                       \
		variant_match(*type) {                                                                  \
			variant_case(PrimitiveType, primitive_type) {                                       \
				if (primitive_type.size != (BIT_COUNT / 8))                                     \
					throw InvalidArgumentSizeError(*global);                                    \
				primitive_args.push_back(primitive_type);                                       \
			}                                                                                   \
			variant_default { throw InvalidArgumentTypeError(*global); }                        \
		}                                                                                       \
	}
				GLOBAL_CASE(8);
				GLOBAL_CASE(16);
				GLOBAL_CASE(32);
				GLOBAL_CASE(64);

				variant_case(CRef<opargs::GlobalPtr>, global) {
					if (!globals.contains(global->global_data_name))
						throw UnknownGlobalNameError(*global);
					CRef<GlobalData> entry = globals.at(global->global_data_name);
					auto             type  = tod_map.at(entry->type);
					if (!std::holds_alternative<PointerType>(*type))
						throw InvalidArgumentTypeError(*global);
				}
				variant_case(CRef<opargs::GlobalOpq>, global_opq) {
					if (!globals.contains(global_opq->global_data_name))
						throw UnknownGlobalNameError(*global_opq);
					CRef<GlobalData> entry = globals.at(global_opq->global_data_name);
					auto             type  = tod_map.at(entry->type);
					if (!std::holds_alternative<OpaqueType>(*type))
						throw InvalidArgumentTypeError(*global_opq);
				}

				STACK_LOCAL_CASE(8);
				STACK_LOCAL_CASE(16);
				STACK_LOCAL_CASE(32);
				STACK_LOCAL_CASE(64);
				variant_case(CRef<opargs::StackLocalPtr>, local) {
					if (!current_stack.contains(local->var_name))
						throw UnknownLocalNameError(*local);
					CRef<TypeOfData> type = current_stack.at(local->var_name);
					if (!std::holds_alternative<PointerType>(*type))
						throw InvalidArgumentTypeError(*local);
				}
				variant_case(CRef<opargs::StackLocalAny>, local) {
					instr_match(instruction) {
						instr_case_novalue(Op_init_lany_type) {
							if (current_stack.contains(local->var_name))
								throw DuplicatedLocalNameError(*local);
						}
						variant_default {
							if (!current_stack.contains(local->var_name))
								throw UnknownLocalNameError(*local);
						}
					}
				}
				variant_case(CRef<opargs::StackLocalOpq>, local) {
					if (!current_stack.contains(local->var_name))
						throw UnknownLocalNameError(*local);
					CRef<TypeOfData> type = current_stack.at(local->var_name);
					if (!std::holds_alternative<OpaqueType>(*type))
						throw InvalidArgumentTypeError(*local);
				}
				variant_case_novalue(CRef<opargs::Immediate>) {}
				variant_case(CRef<opargs::Type>, type_value) {
					if (!tod_map.contains(type_value->type_name))
						throw UnknownTypeError(*type_value);
				}
				variant_case(CRef<opargs::FunctionName>, function_value) {
					auto fun_name = function_value->function_name;
					if (!signatures.contains(fun_name)) throw UnknownFunctionError(*function_value);
				}
				variant_case(CRef<opargs::BuiltinFunctionName>, function_value) {
					auto fun_name = function_value->function_name;
					if (!builtins::isBuiltinFunction(fun_name))
						throw InvalidBuiltinFunctionError(*function_value);
				}
				variant_case(CRef<opargs::ExtCFunctionName>, function_value) {
					auto fun_name = function_value->function_name;
					if (!ext_c_signatures.contains(fun_name))
						throw UnknownFunctionError(*function_value);
				}
				variant_case(CRef<opargs::MethodName>, method_value) {
					auto method_name = method_value->method_name;
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
					if (!valid) throw UnknownMethodError(*method_value);
				}
				variant_case(CRef<opargs::Label>, label_value) {
					instr_match(instruction) {
						instr_case_novalue(Op_label) {}
						// The default instruction for a label argument is a jump instruction.
						instr_default {
							if (!index_of_label.contains(label_value->label_name))
								throw UnknownLabelError(*label_value);
						}
					}
				}

				variant_case(CRef<opargs::StackLocalVnt>, variant) {
					if (!current_stack.contains(variant->var_name))
						throw UnknownLocalNameError(*variant);
					CRef<TypeOfData> type = current_stack.at(variant->var_name);
					if (!std::holds_alternative<VariantType>(*type))
						throw InvalidArgumentTypeError(*variant);
				}

				variant_case(CRef<opargs::Field>, field) {
					auto type = tod_map.at(field->type_name);
					variant_match(*type) {
						variant_case(DataType, ztruct) {
							if (std::ranges::find(ztruct.fields, field->field_name, &Field::name)
							    == ztruct.fields.end())
								throw UnknownFieldError(*field);
						}
						variant_default { throw InvalidArgumentTypeError(*field); }
					}
				}

				// All possible opargs must be handled. Unhandled opargs panic.
				variant_default {
					CORE_PANIC("Unhandled argument case during validation: ", argumentToString(arg));
				}
			}
		}

		// We assert no cross-type operations on primitive types.
		if (primitive_args.size() >= 2) {
			bool sizes_match = std::ranges::all_of(primitive_args, [&](auto x) {
				return x.size == primitive_args.front().size;
			});
			bool names_match = std::ranges::all_of(primitive_args, [&](auto x) {
				return x.name == primitive_args.front().name;
			});
			if (sizes_match && !names_match) throw ArgumentMismatchError(instruction);
		}
	}

	/**
	 * @brief Validates instruction's arguments non-trivially - using specific logic for each
	 * instruction. For instance, an instruction may expect type `T` as arg0, a `Pointer<T>` as
	 * arg1 and another `Pointer<T>` as arg2. This is the place to express such logic.
	 * @param instruction Instruction that is validated.
	 */
	void validateArgTypesNonTrivially(
		const Instruction& instruction, const LocalStack& current_stack
	) const {
		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			instr_case(Op_init_lany_type, instr) { validateArgInstantiable(instr.type); }
			instr_case(Op_alloc_lptr_type, instr) {
				validateArgInstantiable(instr.type);
				CRef<TypeOfData> variable = current_stack.at(instr.ptr.var_name);
				PointerType      pointer  = std::get<PointerType>(*variable);
				if (pointer.inner != instr.type.type_name) throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_upcast_lptr_lptr, instr) { validateUpcast(instr, current_stack); }
			instr_case(Op_cast_l8_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case(Op_cast_l16_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case(Op_cast_l32_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case(Op_cast_l64_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}

			instr_case_novalue(Comment) {}
			instr_case(Op_mov_l8_imm, instr) {
				if (instr.dst.var_name == base::StrID("ret_val")
				    && function.signature.result_type.str == base::StrID("void"))
					throw VoidRetValAssignmentError(instr);
			}
			instr_case(Op_mov_l8_l8, instr) {
				if (instr.dst.var_name == base::StrID("ret_val")
				    && function.signature.result_type.str == base::StrID("void"))
					throw VoidRetValAssignmentError(instr);
			}
			instr_case(Op_cmov_l8_l8, instr) {
				if (instr.dst.var_name == base::StrID("ret_val")
				    && function.signature.result_type.str == base::StrID("void"))
					throw VoidRetValAssignmentError(instr);
			}
			instr_case(Op_cmov_l8_imm, instr) {
				if (instr.dst.var_name == base::StrID("ret_val")
				    && function.signature.result_type.str == base::StrID("void"))
					throw VoidRetValAssignmentError(instr);
			}
			instr_case_novalue(Op_mov_l16_imm) {}
			instr_case_novalue(Op_mov_l16_l16) {}
			instr_case_novalue(Op_cmov_l16_l16) {}
			instr_case_novalue(Op_cmov_l16_imm) {}
			instr_case_novalue(Op_mov_l32_imm) {}
			instr_case_novalue(Op_mov_l32_l32) {}
			instr_case_novalue(Op_cmov_l32_l32) {}
			instr_case_novalue(Op_cmov_l32_imm) {}
			instr_case_novalue(Op_mov_l64_imm) {}
			instr_case_novalue(Op_mov_l64_l64) {}
			instr_case_novalue(Op_cmov_l64_l64) {}
			instr_case_novalue(Op_cmov_l64_imm) {}
			instr_case_novalue(Op_mov_g64_g64) {}
			instr_case_novalue(Op_mov_g64_l64) {}
			instr_case_novalue(Op_mov_g64_imm) {}
			instr_case_novalue(Op_mov_g32_g32) {}
			instr_case_novalue(Op_mov_g32_l32) {}
			instr_case_novalue(Op_mov_g32_imm) {}
			instr_case_novalue(Op_mov_g16_g16) {}
			instr_case_novalue(Op_mov_g16_l16) {}
			instr_case_novalue(Op_mov_g16_imm) {}
			instr_case_novalue(Op_mov_g8_g8) {}
			instr_case_novalue(Op_mov_g8_l8) {}
			instr_case_novalue(Op_mov_g8_imm) {}
			instr_case_novalue(Op_mov_gptr_lptr) {}
			instr_case_novalue(Op_mov_l64_g64) {}
			instr_case_novalue(Op_mov_l32_g32) {}
			instr_case_novalue(Op_mov_l16_g16) {}
			instr_case_novalue(Op_mov_l8_g8) {}
			instr_case_novalue(Op_mov_lptr_gptr) {}
			instr_case_novalue(Op_mov_lptr_lptr) {}
			instr_case_novalue(Op_mov_lopq_lopq) {}
			instr_case_novalue(Op_mov_lopq_gopq) {}
			instr_case_novalue(Op_mov_gopq_lopq) {}
			instr_case_novalue(Op_setNull_lptr) {}

			instr_case_novalue(Op_add_l64_l64) {}
			instr_case_novalue(Op_add_l64_imm) {}
			instr_case_novalue(Op_sub_l64_l64) {}
			instr_case_novalue(Op_sub_l64_imm) {}
			instr_case_novalue(Op_mul_l64_l64) {}
			instr_case_novalue(Op_mul_l64_imm) {}
			instr_case_novalue(Op_mod_l64_l64) {}
			instr_case_novalue(Op_mod_l64_imm) {}
			instr_case_novalue(Op_div_l64_l64) {}
			instr_case_novalue(Op_div_l64_imm) {}
			instr_case_novalue(Op_neg_l64) {}

			instr_case_novalue(Op_add_l32_l32) {}
			instr_case_novalue(Op_add_l32_imm) {}
			instr_case_novalue(Op_sub_l32_l32) {}
			instr_case_novalue(Op_sub_l32_imm) {}
			instr_case_novalue(Op_mul_l32_l32) {}
			instr_case_novalue(Op_mul_l32_imm) {}
			instr_case_novalue(Op_mod_l32_l32) {}
			instr_case_novalue(Op_mod_l32_imm) {}
			instr_case_novalue(Op_div_l32_l32) {}
			instr_case_novalue(Op_div_l32_imm) {}
			instr_case_novalue(Op_neg_l32) {}

			instr_case_novalue(Op_add_l16_l16) {}
			instr_case_novalue(Op_add_l16_imm) {}
			instr_case_novalue(Op_sub_l16_l16) {}
			instr_case_novalue(Op_sub_l16_imm) {}
			instr_case_novalue(Op_mul_l16_l16) {}
			instr_case_novalue(Op_mul_l16_imm) {}
			instr_case_novalue(Op_mod_l16_l16) {}
			instr_case_novalue(Op_mod_l16_imm) {}
			instr_case_novalue(Op_div_l16_l16) {}
			instr_case_novalue(Op_div_l16_imm) {}
			instr_case_novalue(Op_neg_l16) {}

			instr_case_novalue(Op_add_l8_l8) {}
			instr_case_novalue(Op_add_l8_imm) {}
			instr_case_novalue(Op_sub_l8_l8) {}
			instr_case_novalue(Op_sub_l8_imm) {}
			instr_case_novalue(Op_mul_l8_l8) {}
			instr_case_novalue(Op_mul_l8_imm) {}
			instr_case_novalue(Op_mod_l8_l8) {}
			instr_case_novalue(Op_mod_l8_imm) {}
			instr_case_novalue(Op_div_l8_l8) {}
			instr_case_novalue(Op_div_l8_imm) {}
			instr_case_novalue(Op_neg_l8) {}

			instr_case_novalue(Op_cmpEq_l64_l64) {}
			instr_case_novalue(Op_cmpEq_l64_imm) {}
			instr_case_novalue(Op_cmpNeq_l64_l64) {}
			instr_case_novalue(Op_cmpNeq_l64_imm) {}
			instr_case_novalue(Op_cmpEq_l32_l32) {}
			instr_case_novalue(Op_cmpEq_l32_imm) {}
			instr_case_novalue(Op_cmpNeq_l32_l32) {}
			instr_case_novalue(Op_cmpNeq_l32_imm) {}
			instr_case_novalue(Op_cmpEq_l16_l16) {}
			instr_case_novalue(Op_cmpEq_l16_imm) {}
			instr_case_novalue(Op_cmpNeq_l16_l16) {}
			instr_case_novalue(Op_cmpNeq_l16_imm) {}
			instr_case_novalue(Op_cmpEq_l8_l8) {}
			instr_case_novalue(Op_cmpEq_l8_imm) {}
			instr_case_novalue(Op_cmpNeq_l8_l8) {}
			instr_case_novalue(Op_cmpNeq_l8_imm) {}

			instr_case_novalue(Op_cmpGt_l64_l64) {}
			instr_case_novalue(Op_cmpGt_l64_imm) {}
			instr_case_novalue(Op_cmpGe_l64_l64) {}
			instr_case_novalue(Op_cmpGe_l64_imm) {}
			instr_case_novalue(Op_cmpGt_l32_l32) {}
			instr_case_novalue(Op_cmpGt_l32_imm) {}
			instr_case_novalue(Op_cmpGe_l32_l32) {}
			instr_case_novalue(Op_cmpGe_l32_imm) {}
			instr_case_novalue(Op_cmpGt_l16_l16) {}
			instr_case_novalue(Op_cmpGt_l16_imm) {}
			instr_case_novalue(Op_cmpGe_l16_l16) {}
			instr_case_novalue(Op_cmpGe_l16_imm) {}
			instr_case_novalue(Op_cmpGt_l8_l8) {}
			instr_case_novalue(Op_cmpGt_l8_imm) {}
			instr_case_novalue(Op_cmpGe_l8_l8) {}
			instr_case_novalue(Op_cmpGe_l8_imm) {}

			instr_case_novalue(Op_ucmpGt_l64_l64) {}
			instr_case_novalue(Op_ucmpGt_l64_imm) {}
			instr_case_novalue(Op_ucmpGe_l64_l64) {}
			instr_case_novalue(Op_ucmpGe_l64_imm) {}
			instr_case_novalue(Op_ucmpGt_l32_l32) {}
			instr_case_novalue(Op_ucmpGt_l32_imm) {}
			instr_case_novalue(Op_ucmpGe_l32_l32) {}
			instr_case_novalue(Op_ucmpGe_l32_imm) {}
			instr_case_novalue(Op_ucmpGt_l16_l16) {}
			instr_case_novalue(Op_ucmpGt_l16_imm) {}
			instr_case_novalue(Op_ucmpGe_l16_l16) {}
			instr_case_novalue(Op_ucmpGe_l16_imm) {}
			instr_case_novalue(Op_ucmpGt_l8_l8) {}
			instr_case_novalue(Op_ucmpGt_l8_imm) {}
			instr_case_novalue(Op_ucmpGe_l8_l8) {}
			instr_case_novalue(Op_ucmpGe_l8_imm) {}

			instr_case_novalue(Op_cmpLt_l64_l64) {}
			instr_case_novalue(Op_cmpLt_l64_imm) {}
			instr_case_novalue(Op_cmpLe_l64_l64) {}
			instr_case_novalue(Op_cmpLe_l64_imm) {}
			instr_case_novalue(Op_cmpLt_l32_l32) {}
			instr_case_novalue(Op_cmpLt_l32_imm) {}
			instr_case_novalue(Op_cmpLe_l32_l32) {}
			instr_case_novalue(Op_cmpLe_l32_imm) {}
			instr_case_novalue(Op_cmpLt_l16_l16) {}
			instr_case_novalue(Op_cmpLt_l16_imm) {}
			instr_case_novalue(Op_cmpLe_l16_l16) {}
			instr_case_novalue(Op_cmpLe_l16_imm) {}
			instr_case_novalue(Op_cmpLt_l8_l8) {}
			instr_case_novalue(Op_cmpLt_l8_imm) {}
			instr_case_novalue(Op_cmpLe_l8_l8) {}
			instr_case_novalue(Op_cmpLe_l8_imm) {}

			instr_case_novalue(Op_ucmpLt_l64_l64) {}
			instr_case_novalue(Op_ucmpLt_l64_imm) {}
			instr_case_novalue(Op_ucmpLe_l64_l64) {}
			instr_case_novalue(Op_ucmpLe_l64_imm) {}
			instr_case_novalue(Op_ucmpLt_l32_l32) {}
			instr_case_novalue(Op_ucmpLt_l32_imm) {}
			instr_case_novalue(Op_ucmpLe_l32_l32) {}
			instr_case_novalue(Op_ucmpLe_l32_imm) {}
			instr_case_novalue(Op_ucmpLt_l16_l16) {}
			instr_case_novalue(Op_ucmpLt_l16_imm) {}
			instr_case_novalue(Op_ucmpLe_l16_l16) {}
			instr_case_novalue(Op_ucmpLe_l16_imm) {}
			instr_case_novalue(Op_ucmpLt_l8_l8) {}
			instr_case_novalue(Op_ucmpLt_l8_imm) {}
			instr_case_novalue(Op_ucmpLe_l8_l8) {}
			instr_case_novalue(Op_ucmpLe_l8_imm) {}

			instr_case_novalue(Op_fcmpEq_l64_l64) {}
			instr_case_novalue(Op_fcmpEq_l64_imm) {}
			instr_case_novalue(Op_fcmpNeq_l64_l64) {}
			instr_case_novalue(Op_fcmpNeq_l64_imm) {}
			instr_case_novalue(Op_fcmpGt_l64_l64) {}
			instr_case_novalue(Op_fcmpGt_l64_imm) {}
			instr_case_novalue(Op_fcmpGe_l64_l64) {}
			instr_case_novalue(Op_fcmpGe_l64_imm) {}
			instr_case_novalue(Op_fcmpLt_l64_l64) {}
			instr_case_novalue(Op_fcmpLt_l64_imm) {}
			instr_case_novalue(Op_fcmpLe_l64_l64) {}
			instr_case_novalue(Op_fcmpLe_l64_imm) {}

			instr_case_novalue(Op_fcmpEq_l32_l32) {}
			instr_case_novalue(Op_fcmpEq_l32_imm) {}
			instr_case_novalue(Op_fcmpNeq_l32_l32) {}
			instr_case_novalue(Op_fcmpNeq_l32_imm) {}
			instr_case_novalue(Op_fcmpGt_l32_l32) {}
			instr_case_novalue(Op_fcmpGt_l32_imm) {}
			instr_case_novalue(Op_fcmpGe_l32_l32) {}
			instr_case_novalue(Op_fcmpGe_l32_imm) {}
			instr_case_novalue(Op_fcmpLt_l32_l32) {}
			instr_case_novalue(Op_fcmpLt_l32_imm) {}
			instr_case_novalue(Op_fcmpLe_l32_l32) {}
			instr_case_novalue(Op_fcmpLe_l32_imm) {}

			instr_case_novalue(Op_cmpNull_lptr) {}

			instr_case_novalue(Op_fadd_l64_l64) {}
			instr_case_novalue(Op_fadd_l64_imm) {}
			instr_case_novalue(Op_fadd_l32_l32) {}
			instr_case_novalue(Op_fadd_l32_imm) {}

			instr_case_novalue(Op_fsub_l64_l64) {}
			instr_case_novalue(Op_fsub_l64_imm) {}
			instr_case_novalue(Op_fsub_l32_l32) {}
			instr_case_novalue(Op_fsub_l32_imm) {}

			instr_case_novalue(Op_fmul_l64_l64) {}
			instr_case_novalue(Op_fmul_l64_imm) {}
			instr_case_novalue(Op_fmul_l32_l32) {}
			instr_case_novalue(Op_fmul_l32_imm) {}

			instr_case_novalue(Op_fdiv_l64_l64) {}
			instr_case_novalue(Op_fdiv_l64_imm) {}
			instr_case_novalue(Op_fdiv_l32_l32) {}
			instr_case_novalue(Op_fdiv_l32_imm) {}

			instr_case_novalue(Op_fneg_l64) {}
			instr_case_novalue(Op_fneg_l32) {}

			instr_case_novalue(Op_umul_l64_l64) {}
			instr_case_novalue(Op_umul_l64_imm) {}
			instr_case_novalue(Op_umul_l32_l32) {}
			instr_case_novalue(Op_umul_l32_imm) {}
			instr_case_novalue(Op_umul_l16_l16) {}
			instr_case_novalue(Op_umul_l16_imm) {}
			instr_case_novalue(Op_umod_l64_l64) {}
			instr_case_novalue(Op_umod_l64_imm) {}
			instr_case_novalue(Op_umod_l32_l32) {}
			instr_case_novalue(Op_umod_l32_imm) {}
			instr_case_novalue(Op_umod_l16_l16) {}
			instr_case_novalue(Op_umod_l16_imm) {}
			instr_case_novalue(Op_udiv_l64_l64) {}
			instr_case_novalue(Op_udiv_l64_imm) {}
			instr_case_novalue(Op_udiv_l32_l32) {}
			instr_case_novalue(Op_udiv_l32_imm) {}
			instr_case_novalue(Op_udiv_l16_l16) {}
			instr_case_novalue(Op_udiv_l16_imm) {}
			instr_case_novalue(Op_log_and_l8_l8) {}
			instr_case_novalue(Op_log_and_l8_imm) {}
			instr_case_novalue(Op_log_or_l8_l8) {}
			instr_case_novalue(Op_log_or_l8_imm) {}
			instr_case_novalue(Op_log_xor_l8_l8) {}
			instr_case_novalue(Op_log_xor_l8_imm) {}
			instr_case_novalue(Op_log_not_l8) {}


			instr_case(Op_variantSetInner_lvnt_type, instr) {
				const auto& variant_type
					= std::get<VariantType>(*current_stack.at(instr.variant.var_name));
				base::StrID                     wanted_type    = instr.inner_type.type_name;
				const std::vector<base::StrID>& possible_types = variant_type.variant_alternatives;
				if (!std::ranges::contains(possible_types, wanted_type))
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantGetInner_lptr_lvnt_type, instr) {
				const auto& variant_type
					= std::get<VariantType>(*current_stack.at(instr.variant.var_name));
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));
				base::StrID                     wanted_type    = pointer_type.inner;
				const std::vector<base::StrID>& possible_types = variant_type.variant_alternatives;
				if (!std::ranges::contains(possible_types, wanted_type))
					throw VariantTypeMismatchError(instr);

				if (instr.expected_type != wanted_type) throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantSetInner_lptr_type, instr) {
				const auto& variant_pointer
					= std::get<PointerType>(*current_stack.at(instr.variant_ptr.var_name));

				const auto& variant_type
					= expectPointerType<VariantType>(variant_pointer, tod_map, instr);

				base::StrID wanted_type = instr.inner_type.type_name;
				if (!std::ranges::contains(variant_type.variant_alternatives, wanted_type))
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantGetInner_lptr_lptr_type, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));
				base::StrID wanted_type = pointer_type.inner;

				const auto& variant_pointer
					= std::get<PointerType>(*current_stack.at(instr.variant_ptr.var_name));
				const auto& variant_type
					= expectPointerType<VariantType>(variant_pointer, tod_map, instr);

				if (!std::ranges::contains(variant_type.variant_alternatives, wanted_type))
					throw VariantTypeMismatchError(instr);

				if (instr.expected_type != wanted_type) throw VariantTypeMismatchError(instr);
			}
			instr_case_novalue(Op_label) {}
			instr_case_novalue(Op_jmp_label) {}
			instr_case_novalue(Op_jmpIf_label) {}
			instr_case_novalue(Op_jmpIfNot_label) {}
			instr_case_novalue(Op_call_func) {}
			instr_case_novalue(Op_call_builtinfunc) {}
			instr_case_novalue(Op_call_cfunc) {}
			instr_case(Op_virtual_call_lptr_method, instr) {
				// For a method all to be valid, the called method has to be declared as a virtual
				// method in this inheritable or it's superclasses or interfaces.
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.object_ptr.var_name));
				const auto& inh_meta
					= type_metadata.at(pointer_type.inner)->getInheritanceMetadata();

				const auto& obj_type = type_metadata.at(pointer_type.inner);

				bool                          valid                  = false;
				std::function<void(TypeCRef)> check_for_superclasses = [&](TypeCRef inh_type) {
					if (valid) return;
					if_opt_some(inh_type->getInheritanceMetadata(), imd) {
						if (imd->virtual_methods.contains(instr.method.method_name)) {
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
			instr_case_novalue(Op_ret_tailcall_func) {}
			instr_case_novalue(Op_ret) {}
			instr_case_novalue(Op_deinit) {}
			instr_case_novalue(Op_input_l64) {}
			instr_case_novalue(Op_output_l64) {}
			instr_case_novalue(Op_input_l32) {}
			instr_case_novalue(Op_output_l32) {}
			instr_case(Op_setVTable_lptr_type, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.object_ptr.var_name));
				if (pointer_type.inner != instr.type.type_name)
					throw VTableTypeMismatchError(instr);
			}
			instr_case_novalue(Op_downcast_lptr_lptr_type) {}
			instr_case_novalue(Op_free_lptr) {}
			instr_case(Op_store_lptr_lany, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));
				CRef<TypeOfData> other_type      = current_stack.at(instr.src.var_name);
				base::StrID      other_type_name = typeName(*other_type);
				if (pointer_type.inner != other_type_name) throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_load_lany_lptr, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.src_ptr.var_name));
				CRef<TypeOfData> other_type      = current_stack.at(instr.dst.var_name);
				base::StrID      other_type_name = typeName(*other_type);
				if (pointer_type.inner != other_type_name) throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_ref_lptr_lany, instr) {
				const auto& pointer_type
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));
				CRef<TypeOfData> other_type      = current_stack.at(instr.src.var_name);
				base::StrID      other_type_name = typeName(*other_type);
				if (pointer_type.inner != other_type_name) throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_structLea_lptr_lptr_field, instr) {
				const auto& destination
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));

				const auto& ztruct_pointer
					= std::get<PointerType>(*current_stack.at(instr.src_data_ptr.var_name));
				const auto& ztruct = expectPointerType<DataType>(ztruct_pointer, tod_map, instr);

				validateStructFieldType(ztruct, instr.field, destination.inner, instr);
			}
			instr_case(Op_structLoad_lany_lptr_field, instr) {
				const auto& destination = current_stack.at(instr.dst.var_name);

				const auto& ztruct_pointer
					= std::get<PointerType>(*current_stack.at(instr.src_data_ptr.var_name));
				const auto& ztruct = expectPointerType<DataType>(ztruct_pointer, tod_map, instr);

				validateStructFieldType(ztruct, instr.field, typeName(*destination), instr);
			}
			instr_case(Op_structStore_lptr_lany_field, instr) {
				const auto& source = current_stack.at(instr.src.var_name);

				const auto& ztruct_pointer
					= std::get<PointerType>(*current_stack.at(instr.dst_data_ptr.var_name));
				const auto& ztruct = expectPointerType<DataType>(ztruct_pointer, tod_map, instr);

				validateStructFieldType(ztruct, instr.field, typeName(*source), instr);
			}
			instr_case(Op_fixedSizeTableLea_lptr_lptr_l64, instr) {
				const auto& destination
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.src_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<FixedSizeTableType>(table_pointer, tod_map, instr);

				if (destination.inner != table_type.inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableLea_lptr_lptr_l64, instr) {
				const auto& destination
					= std::get<PointerType>(*current_stack.at(instr.dst_ptr.var_name));

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.src_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<DynamicTableType>(table_pointer, tod_map, instr);

				if (destination.inner != table_type.inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableLoad_lany_lptr_l64, instr) {
				const auto& destination = current_stack.at(instr.dst.var_name);

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.src_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<FixedSizeTableType>(table_pointer, tod_map, instr);

				if (typeName(*destination) != table_type.inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableLoad_lany_lptr_l64, instr) {
				const auto& destination = current_stack.at(instr.dst.var_name);
				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.src_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<DynamicTableType>(table_pointer, tod_map, instr);
				if (typeName(*destination) != table_type.inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableStore_lptr_lany_l64, instr) {
				const auto& source = current_stack.at(instr.src.var_name);

				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.dst_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<FixedSizeTableType>(table_pointer, tod_map, instr);

				if (table_type.inner != typeName(*source))
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableStore_lptr_lany_l64, instr) {
				const auto& source = current_stack.at(instr.src.var_name);
				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.dst_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<DynamicTableType>(table_pointer, tod_map, instr);

				if (table_type.inner != typeName(*source))
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableReAlloc_lptr_type_l64, instr) {
				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.dst_table_ptr.var_name));
				const auto& table_type
					= expectPointerType<DynamicTableType>(table_pointer, tod_map, instr);

				base::StrID wanted_type = instr.table_type.type_name;
				if (wanted_type != table_type.name)
					throw InvalidArgumentTypeError(instr.table_type);
			}
			instr_case(Op_strOutput_lptr, instr) {
				const auto& table_pointer
					= std::get<PointerType>(*current_stack.at(instr.string_ptr.var_name));
				const auto& table_type
					= expectPointerType<DynamicTableType>(table_pointer, tod_map, instr);
				if (table_type.inner != "byte") throw DynamicTableTypeMismatchError(instr);
			}
			instr_case_novalue(Op_nop) {}
			instr_case_novalue(Op_exit) {}
			instr_case_novalue(Op_breakpoint) {}
			instr_case_novalue(Op_initFromVmValue) {}
		}
		POP_DIAGNOSTIC
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
		auto dst_ptr_tod = current_stack.at(instruction.dst.var_name);
		auto src_ptr_tod = current_stack.at(instruction.src.var_name);

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
		        && !std::ranges::contains(VALID_LAST_OPCODES, function.body.back().opcode()))) {
			throw PathWithoutEndError(function.name);
		}
	}

	usize getLabelTarget(const opargs::Label& label) const {
		return index_of_label.at(label.label_name);
	}

	void preprocessLabels() {
		auto register_jump = [&](const auto& instr) {
			jumps_to_label.try_emplace(instr.label.label_name);
			jumps_to_label.at(instr.label.label_name).push_back(instr);
		};

		for (usize index = 0; index < function.body.size(); index++) {
			instr_match(function.body[index]) {
				instr_case(Op_label, instr) {
					auto [_, added]
						= index_of_label.insert_or_assign(instr.label.label_name, index);
					if (!added) throw DuplicatedLabelError(instr.label);
				}
				instr_case(Op_jmp_label, instr) { register_jump(instr); }
				instr_case(Op_jmpIf_label, instr) { register_jump(instr); }
				instr_case(Op_jmpIfNot_label, instr) { register_jump(instr); }
				instr_default {}
			}
		}
	}

	void traverseControlFlowGraph() {
		LocalStack local_stack(function.signature, tod_map, type_metadata);
		visited_instructions.resize(function.body.size());
		std::vector<std::tuple<usize, LocalStack>> dfs_stack{
			{ function.body.size(), local_stack }  // sentinel
		};
		usize index = 0;

		auto& instructions = function.body;

		while (index != function.body.size()) {
			validateArgTypes(instructions[index], local_stack);

			validateArgTypesNonTrivially(instructions[index], local_stack);

			visited_instructions[index] = true;
			instr_match(instructions[index]) {
				instr_case(Op_init_lany_type, instr) {
					local_stack.push(instr.var, instr.type);
					index++;
				}
				instr_case(Op_deinit, instr) {
					local_stack.pop(instr);
					index++;
				}
				instr_case(Op_label, instr) {
					match_optional(stack_at_label.atMaybe(instr.label.label_name)) {
						opt_some(label_state) {
							if (*label_state != local_stack.getStackState())
								throw StackStructureMismatchError(
									instr, jumps_to_label.at(instr.label.label_name)
								);
							std::tie(index, local_stack) = dfs_stack.back();
							dfs_stack.pop_back();
						}
						opt_none {
							stack_at_label.put(instr.label.label_name, local_stack.getStackState());
							index++;
						}
					}
				}
				instr_case(Op_jmp_label, instr) { index = getLabelTarget(instr.label); }
				instr_case(Op_jmpIf_label, instr) {
					index++;
					dfs_stack.emplace_back(getLabelTarget(instr.label), local_stack);
				}
				instr_case(Op_jmpIfNot_label, instr) {
					index++;
					dfs_stack.emplace_back(getLabelTarget(instr.label), local_stack);
				}
				instr_case(Op_ret, instr) {
					std::tie(index, local_stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_case(Op_call_func, instr) {
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_call_builtinfunc, instr) {
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_call_cfunc, instr) {
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_virtual_call_lptr_method, instr) {
					validateMethodCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_ret_tailcall_func, instr) {
					validateTailcall(local_stack, instr, function.signature);
					std::tie(index, local_stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
#define HANDLE_CAST(SIZE)                                          \
	instr_case(Op_cast_l##SIZE##_type, instr) {                    \
		local_stack.castPrimitive(instr.value, instr.target_type); \
		index++;                                                   \
	}

				FOR_EACH(HANDLE_CAST, 8, 16, 32, 64)
#undef HANDLE_CAST
				instr_default { index++; }
			}
		}
	}

	void validateSignature() {
		for (const auto& param_type: function.signature.parameters) {
			if (!tod_map.contains(param_type)) throw UnknownTypeError(opargs::Type{ param_type });
			if (param_type.str == base::StrID("void")) throw VoidTypeArgumentError(function.name);
		}
		if (!tod_map.contains(function.signature.result_type.str))
			throw UnknownTypeError(opargs::Type{ function.signature.result_type.str });
	}

public:
	FunctionValidator(
		const ObjIdNameMap<TypeOfData>&                  tod_map,
		const TypeMetadata&                              type_metadata,
		const ObjIdNameMap<GlobalData>&                  globals,
		const base::HashMap<base::StrID, FuncSignature>& signatures,
		const ObjIdNameMap<ExternalCFunction>&           ext_c_signatures,
		const Function&                                  function
	):
		  tod_map(tod_map),
		  type_metadata(type_metadata),
		  globals(globals),
		  signatures(signatures),
		  ext_c_signatures(ext_c_signatures),
		  function(function) {}

	std::vector<Instruction> validateAndExtractReachableCode() {
		validateSignature();
		preprocessLabels();
		traverseControlFlowGraph();
		validateFunctionEnd();

		std::vector<Instruction> out;
		for (auto [instruction, visited]: std::views::zip(function.body, visited_instructions))
			if (visited) out.push_back(instruction);
		return out;
	}
};

vm::code::Function vm::code::detail::validateAndExtractReachableCode(
	const ObjIdNameMap<TypeOfData>&                  tod_map,
	const TypeMetadata&                              type_metadata,
	const ObjIdNameMap<GlobalData>&                  globals_map,
	const base::HashMap<base::StrID, FuncSignature>& signatures,
	const ObjIdNameMap<ExternalCFunction>&           ext_c_signatures,
	const Function&                                  function
) {
	FuncSignature signature = signatures.at(function.name);

	FunctionValidator validator(
		tod_map, type_metadata, globals_map, signatures, ext_c_signatures, function
	);

	Function new_function;
	new_function.name         = function.name;
	new_function.body         = validator.validateAndExtractReachableCode();
	new_function.bytecode_pos = function.bytecode_pos;
	new_function.signature    = signature;

	return new_function;
}
