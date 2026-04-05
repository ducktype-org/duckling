#include "function_validator.hpp"

#include "errors.hpp"

#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/process/builtin_functions.hpp>
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
	concept VmType = base::IsVariantMember<T, TypeOfData>;

	template<typename T>
	concept CallingInstruction = base::IsTupleMember<T, CallingInstructions>;

	template<valid_type::ConcreteType ExpectedT, class ErrorT = PointerTypeMismatchError, class... Args>
	CRef<ExpectedT> expectPointerType(
		CRef<vm::code::valid_type::finalized::Pointer> pointer,
		const valid_type::ValidTypeMap&                types_ctx,
		Args&&... error_args
	) {
		const auto& pointed_type = types_ctx.at(pointer->inner);
		return pointed_type->maybeGetKindAs<ExpectedT>().template expect<ErrorT>(
			std::forward<Args>(error_args)...
		);
	}

	template<class ErrorT = FieldTypeMismatchError, class... Args>
	void validateStructFieldType(
		const valid_type::ValidType&            type,
		const valid_type::finalized::Structure& as_struct,
		const opargs::Field&                    field_arg,
		valid_type::ValidTypeID                 expected_field_type,
		Args&&... error_args
	) {
		if (type.getName() != field_arg.type_name)
			throw StructTypeMismatchError(std::forward<Args>(error_args)...);

		base::StrID                  field_name = field_arg.field_name;
		valid_type::finalized::Field field
			= *std::ranges::find(as_struct.fields, field_name, &valid_type::finalized::Field::name);

		if (field.type != expected_field_type) throw ErrorT(std::forward<Args>(error_args)...);
	}
}

/**
 * @brief Represents a local stack variable.
 */
struct LocalStackEntry {
	base::StrID                 local_name;
	CRef<valid_type::ValidType> type;

	constexpr bool operator==(const LocalStackEntry& other) const {
		return local_name == other.local_name && *type == *other.type;
	}
};

class LocalStack {
	std::vector<LocalStackEntry> stack_state;

	// the following are CRefs instead of const& to allow copy/move.

	CRef<valid_type::ValidTypeMap>                          types_ctx;
	base::HashMap<base::StrID, CRef<valid_type::ValidType>> local_name_to_type;
	usize                                                   number_of_ret_vals = 0;

public:
	LocalStack(const LocalStack&)            = default;
	LocalStack(LocalStack&&)                 = default;
	LocalStack& operator=(const LocalStack&) = default;
	LocalStack& operator=(LocalStack&&)      = default;

	LocalStack(const FuncSignature& signature, const valid_type::ValidTypeMap& types_ctx):
		  types_ctx(&types_ctx),
		  number_of_ret_vals(signature.result_types.size()) {
		using namespace std::views;
		for (auto [idx, ret]: enumerate(signature.result_types))
			push(base::StrID(base::strConcat("ret_val_", idx).c_str()), ret.str);

		for (auto [idx, param]: enumerate(signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param.str);
	}

	const std::vector<LocalStackEntry>& getStackState() const { return stack_state; }

	void push(const opargs::StackLocalAny& local, const opargs::Type& type) {
		auto tp = types_ctx->at(type.type_name);

		if (local_name_to_type.contains(local.var_name)) throw DuplicatedLocalNameError(local);

		stack_state.emplace_back(local.var_name, tp);
		local_name_to_type.put(local.var_name, tp);
	}

	/**
	 * @brief Pops the top element from the stack state and updates local variable mappings.
	 * Can be only used with instructions which effectively deinitialize the local stack
	 * (deinit, call_func, virtual_call and call_builtinfunc)
	 */
	template<DeinitializingInstruction InstructionType>
	void pop(const InstructionType& cause) {
		if (stack_state.size() == number_of_ret_vals) throw RetValDeinitError(cause);
		const auto& top = stack_state.back();
		local_name_to_type.erase(top.local_name);
		stack_state.pop_back();
	}

	usize size() const { return stack_state.size(); }

	const LocalStackEntry& back(usize i = 0) const {
		return stack_state.at(stack_state.size() - 1 - i);
	}

	const LocalStackEntry& front(usize i = 0) const { return stack_state.at(i); }

	void castPrimitive(const opargs::OpCodePrimitiveArg& local, const opargs::Type& type) {
		auto  local_name = VISIT(local, l, return l.var_name);
		auto& curr_type  = local_name_to_type.at(local_name);
		auto  new_type   = types_ctx->at(type.type_name);
		curr_type        = new_type;
		for (auto& entry: stack_state)
			if (entry.local_name == local_name) entry.type = new_type;
	}

	bool contains(base::StrID local_name) const { return local_name_to_type.contains(local_name); }

	CRef<valid_type::ValidType> at(base::StrID local_name) const {
		return local_name_to_type.at(local_name);
	}
};

/**
 * @brief Class responsible for function validation.
 * Processes the control-flow graph and simulates
 * stack operations. Throws subclasses of ValidationError.
 */
class FunctionValidator {
	const valid_type::ValidTypeMap&                  types_ctx;
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

		auto& params = signature->parameters;
		auto& reslts = signature->result_types;

		if (params.size() + reslts.size() > local_stack.size())
			throw InvalidFunctionCallArgumentsError(generic_arg);

		using namespace std::views;
		for (auto param: params | reverse) {
			if (local_stack.back().type->getName() != param.str)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}

		for (auto [idx, reslt]: enumerate(reslts | reverse))
			if (local_stack.back(idx).type->getName() != reslt.str)
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
	 *   them may declare a pointer to a different type (only a pointer to SELF - subclass can
	 * differ). Validating just the pointer type name like in normal function calls would simply
	 * don't work.
	 */
	void validateMethodCallAndPop(LocalStack& local_stack, const Op_virtual_call_lptr_method& instr) {
		// @TODO: #962 This implementation seeking occurs in a couple of places. Think of a better
		// way. EDIT: After valid_type::ValidType was added, it's simpler but still could be improved.
		CRef<valid_type::finalized::Function> method_signature = [&] {
			const auto&             ptr = local_stack.at(instr.object_ptr.var_name);
			valid_type::ValidTypeID inner_id
				= ptr->getKindAs<valid_type::finalized::Pointer>()->inner;
			const auto structure
				= types_ctx.at(inner_id)->getKindAs<valid_type::finalized::Structure>();
			const auto& imd           = structure->inheritance_metadata.value();
			auto        method_type   = imd.available_methods[instr.method.method_name];
			auto        function_type = types_ctx.at(method_type);
			return function_type->getKindAs<valid_type::finalized::Function>();
		}();

		auto generic_arg = opargs::OpCodeArg{ instr.method };

		auto& params = method_signature->parameters;
		auto& reslts = method_signature->result_types;

		// Too many parameters.
		if (params.size() + reslts.size() > local_stack.size())
			throw InvalidFunctionCallArgumentsError(generic_arg);

		using namespace std::views;
		// Check individual parameter's types. The first parameter is special, because it should be
		// a pointer to the same type as the pointer passed as `obj_ptr`.
		for (auto param_id: params | drop(1) | reverse) {
			if (local_stack.back().type->getID() != param_id)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}

		auto       type_name = local_stack.back().type->getName();
		const auto ptr_on_stack
			= types_ctx.at(type_name)->getKindAs<valid_type::finalized::Pointer>();
		const auto& ptr_in_call
			= local_stack.at(instr.object_ptr.var_name)->getKindAs<valid_type::finalized::Pointer>();
		if (ptr_in_call->inner != ptr_on_stack->inner)
			throw InvalidFunctionCallArgumentsError(generic_arg);
		local_stack.pop(instr);

		for (auto [idx, reslt]: enumerate(reslts | reverse))
			if (local_stack.back(idx).type->getID() != reslt)
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

		if (!(signature.result_types == current_signature.result_types
		      && signature.parameters == current_signature.parameters))
			throw InvalidTailcallSignatureError(generic_arg);

		auto& params = signature.parameters;
		auto& reslts = signature.result_types;

		if (params.size() + reslts.size() != local_stack.size())
			throw InvalidTailcallArgumentsError(generic_arg);

		using namespace std::views;
		for (auto [reslt, stack_elem]:
		     zip(reslts, local_stack.getStackState() | take(reslts.size())))

			if (stack_elem.type->getName() != reslt.str)
				throw InvalidTailcallArgumentsError(generic_arg);

		for (auto [param, stack_elem]:
		     zip(params, local_stack.getStackState() | drop(reslts.size())))
			if (stack_elem.type->getName() != param.str)
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
		for (auto arg: instruction.args()) {
			variant_match(arg) {
#define STACK_LOCAL_CASE(BIT_COUNT)                                                        \
	variant_case(CRef<opargs::StackLocal##BIT_COUNT>, local) {                             \
		if (!current_stack.contains(local->var_name)) throw UnknownLocalNameError(*local); \
		CRef<valid_type::ValidType> entry = current_stack.at(local->var_name);             \
		variant_match(entry->getKind()) {                                                  \
			variant_case(valid_type::finalized::Primitive, primitive_type) {               \
				if (base::bytes2bits(primitive_type.size).asInt() != BIT_COUNT)            \
					throw InvalidArgumentSizeError(*local);                                \
			}                                                                              \
			variant_default { throw InvalidArgumentTypeError(*local); }                    \
		}                                                                                  \
	}
#define GLOBAL_CASE(BIT_COUNT)                                                                  \
	variant_case(CRef<opargs::Global##BIT_COUNT>, global) {                                     \
		if (!globals.contains(global->global_data_name)) throw UnknownGlobalNameError(*global); \
		CRef<GlobalData>            entry = globals.at(global->global_data_name);               \
		CRef<valid_type::ValidType> type  = types_ctx.at(entry->type);                          \
		variant_match(type->getKind()) {                                                        \
			variant_case(valid_type::finalized::Primitive, primitive_type) {                    \
				if (base::bytes2bits(primitive_type.size).asInt() != BIT_COUNT)                 \
					throw InvalidArgumentSizeError(*global);                                    \
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
					CRef<GlobalData>            entry = globals.at(global->global_data_name);
					CRef<valid_type::ValidType> type  = types_ctx.at(entry->type);
					if (type->isKind<valid_type::finalized::Primitive>())
						throw InvalidArgumentTypeError(*global);
				}
				variant_case(CRef<opargs::GlobalAny>, global) {
					if (!globals.contains(global->global_data_name))
						throw UnknownGlobalNameError(*global);
				}
				variant_case(CRef<opargs::GlobalOpq>, global_opq) {
					if (!globals.contains(global_opq->global_data_name))
						throw UnknownGlobalNameError(*global_opq);
					CRef<GlobalData>            entry = globals.at(global_opq->global_data_name);
					CRef<valid_type::ValidType> type  = types_ctx.at(entry->type);
					if (!type->isKind<valid_type::finalized::Opaque>())
						throw InvalidArgumentTypeError(*global_opq);
				}

				STACK_LOCAL_CASE(8);
				STACK_LOCAL_CASE(16);
				STACK_LOCAL_CASE(32);
				STACK_LOCAL_CASE(64);
				variant_case(CRef<opargs::StackLocalPtr>, local) {
					if (!current_stack.contains(local->var_name))
						throw UnknownLocalNameError(*local);
					CRef<valid_type::ValidType> type = current_stack.at(local->var_name);
					if (!type->isKind<valid_type::finalized::Pointer>())
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
					CRef<valid_type::ValidType> type = current_stack.at(local->var_name);
					if (!type->isKind<valid_type::finalized::Opaque>())
						throw InvalidArgumentTypeError(*local);
				}
				variant_case_novalue(CRef<opargs::Immediate>) {}
				variant_case(CRef<opargs::Type>, type_value) {
					if (!types_ctx.contains(type_value->type_name))
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
					for (const auto& type: types_ctx) {
						std::visit(
							[&](const auto& t) {
								if constexpr (requires { t.inheritance_metadata; }) {
									if_opt_some(t.inheritance_metadata, inh_meta) {
										for (const auto& vmethod: inh_meta.available_methods)
											if (vmethod.first == method_name) valid = true;
									}
								}
							},
							type.getKind()
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
					CRef<valid_type::ValidType> type = current_stack.at(variant->var_name);
					if (!type->isKind<valid_type::finalized::Variant>())
						throw InvalidArgumentTypeError(*variant);
				}

				variant_case(CRef<opargs::Field>, field) {
					auto type = types_ctx.at(field->type_name);
					variant_match(type->getKind()) {
						variant_case(valid_type::finalized::Structure, structure) {
							if (std::ranges::find(
									structure.fields,
									field->field_name,
									&valid_type::finalized::Field::name
								)
							    == structure.fields.end())
								throw UnknownFieldError(*field);
						}
						variant_default { throw InvalidArgumentTypeError(*field); }
					}
				}

				variant_case(CRef<opargs::StackLocalStructure>, local_struct) {
					if (!current_stack.contains(local_struct->var_name))
						throw UnknownLocalNameError(*local_struct);
					CRef<valid_type::ValidType> type = current_stack.at(local_struct->var_name);
					if (!type->isKind<valid_type::finalized::Structure>())
						throw InvalidArgumentTypeError(*local_struct);
				}

				// All possible opargs must be handled. Unhandled opargs panic.
				variant_default {
					CORE_PANIC("Unhandled argument case during validation: ", argumentToString(arg));
				}
			}
		}
	}

	/**
	 * @brief Validates that all primitive arguments of the instruction are of the same type. Used
	 * for instructions which require this, e.g. `mov_l8_l8` - both arguments must be of the same
	 * primitive type - Invalid combination is (e.g. `i32` and `u32`).
	 * @param instruction Instruction that is validated.
	 */
	void validateStackPrimitiveArgumentsSameType(
		const Instruction& instruction, const LocalStack& current_stack
	) const {
		std::vector<valid_type::ValidTypeID> primitive_args;

		for (auto arg: instruction.args()) {
			variant_match(arg) {
#define STACK_LOCAL_CASE_PRIMITIVE_VALIDATION(BIT_COUNT)                      \
	variant_case(CRef<opargs::StackLocal##BIT_COUNT>, local) {                \
		primitive_args.push_back(current_stack.at(local->var_name)->getID()); \
	}

#define GLOBAL_CASE_PRIMITIVE_VALIDATION(BIT_COUNT)                    \
	variant_case(CRef<opargs::Global##BIT_COUNT>, global) {            \
		CRef<GlobalData> entry = globals.at(global->global_data_name); \
		primitive_args.push_back(types_ctx.at(entry->type)->getID());  \
	}
				FOR_EACH(STACK_LOCAL_CASE_PRIMITIVE_VALIDATION, 8, 16, 32, 64)
				FOR_EACH(GLOBAL_CASE_PRIMITIVE_VALIDATION, 8, 16, 32, 64)
				variant_default { continue; }
			}
		}
		bool ids_match = std::ranges::all_of(primitive_args, [&](auto x) {
			return x == primitive_args.front();
		});
		if (!ids_match) throw ArgumentMismatchError(instruction);
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
				CRef<valid_type::ValidType> variable = current_stack.at(instr.ptr.var_name);
				CRef<valid_type::finalized::Pointer> pointer
					= variable->getKindAs<valid_type::finalized::Pointer>();
				if (types_ctx.at(pointer->inner)->getName() != instr.type.type_name)
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_upcast_lptr_lptr, instr) {
				validateClassCast<InvalidUpcastError>(instr, current_stack);
			}
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
			instr_case(Op_mov_l8_imm, instr) {}
			instr_case(Op_mov_l8_l8, instr) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case(Op_cmov_l8_l8, instr) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case(Op_cmov_l8_imm, instr) {}
			instr_case_novalue(Op_mov_l16_imm) {}
			instr_case_novalue(Op_mov_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_l16_imm) {}
			instr_case_novalue(Op_mov_l32_imm) {}
			instr_case_novalue(Op_mov_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_l32_imm) {}
			instr_case_novalue(Op_mov_l64_imm) {}
			instr_case_novalue(Op_mov_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_l64_imm) {}
			instr_case_novalue(Op_mov_g64_g64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g64_imm) {}
			instr_case_novalue(Op_mov_g32_g32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g32_imm) {}
			instr_case_novalue(Op_mov_g16_g16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g16_imm) {}
			instr_case_novalue(Op_mov_g8_g8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_g8_imm) {}
			instr_case(Op_mov_lptr_lptr, instr) {
				auto src_type = current_stack.at(instr.src.var_name)->getName();
				auto dst_type = current_stack.at(instr.dst.var_name)->getName();
				if (src_type != dst_type) throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_mov_gptr_lptr, instr) {
				auto src_type = current_stack.at(instr.src.var_name)->getName();
				auto dst_type = globals.at(instr.dst.global_data_name)->type.str;
				if (src_type != dst_type) throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_mov_lptr_gptr, instr) {
				auto src_type = globals.at(instr.src.global_data_name)->type.str;
				auto dst_type = current_stack.at(instr.dst.var_name)->getName();
				if (src_type != dst_type) throw PointerTypeMismatchError(instr);
			}
			instr_case_novalue(Op_mov_l64_g64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_l32_g32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_l16_g16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_l8_g8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mov_lopq_lopq) {}
			instr_case_novalue(Op_mov_lopq_gopq) {}
			instr_case_novalue(Op_mov_gopq_lopq) {}
			instr_case_novalue(Op_mov_lopq_imm) {}

			instr_case(Op_mov_lste_lste, instr) {
				// Validate that both sides are the same structs.
				auto dst_type = current_stack.at(instr.dst.var_name)->getName();
				auto src_type = current_stack.at(instr.src.var_name)->getName();
				if (dst_type != src_type) throw StructTypeMismatchError(instr);
			}
			instr_case(Op_mov_lste_gste, instr) {
				// Validate that both sides are the same structs.
				auto dst_type = current_stack.at(instr.dst.var_name)->getName();
				auto src_type = globals.at(instr.src.global_data_name)->type.str;
				if (dst_type != src_type) throw StructTypeMismatchError(instr);
			}
			instr_case(Op_mov_gste_lste, instr) {
				// Validate that both sides are the same structs.
				auto dst_type = globals.at(instr.dst.global_data_name)->type.str;
				auto src_type = current_stack.at(instr.src.var_name)->getName();
				if (dst_type != src_type) throw StructTypeMismatchError(instr);
			}
			instr_case(Op_mov_gste_gste, instr) {
				// Validate that both sides are the same structs.
				auto dst_type = globals.at(instr.dst.global_data_name)->type.str;
				auto src_type = globals.at(instr.src.global_data_name)->type.str;
				if (dst_type != src_type) throw StructTypeMismatchError(instr);
			}

			instr_case_novalue(Op_setNull_lptr) {}

			// Sign Extension
			instr_case_novalue(Op_sext_l16_l8) {}
			instr_case_novalue(Op_sext_l32_l8) {}
			instr_case_novalue(Op_sext_l64_l8) {}
			instr_case_novalue(Op_sext_l32_l16) {}
			instr_case_novalue(Op_sext_l64_l16) {}
			instr_case_novalue(Op_sext_l64_l32) {}

			// Zero Extension
			instr_case_novalue(Op_zext_l16_l8) {}
			instr_case_novalue(Op_zext_l32_l8) {}
			instr_case_novalue(Op_zext_l64_l8) {}
			instr_case_novalue(Op_zext_l32_l16) {}
			instr_case_novalue(Op_zext_l64_l16) {}
			instr_case_novalue(Op_zext_l64_l32) {}

			// Truncation
			instr_case_novalue(Op_trunc_l8_l16) {}
			instr_case_novalue(Op_trunc_l8_l32) {}
			instr_case_novalue(Op_trunc_l8_l64) {}
			instr_case_novalue(Op_trunc_l16_l32) {}
			instr_case_novalue(Op_trunc_l16_l64) {}
			instr_case_novalue(Op_trunc_l32_l64) {}

			// Int to Float
			instr_case_novalue(Op_sitofp_l32_l8) {}
			instr_case_novalue(Op_uitofp_l32_l8) {}
			instr_case_novalue(Op_sitofp_l32_l16) {}
			instr_case_novalue(Op_uitofp_l32_l16) {}
			instr_case_novalue(Op_sitofp_l32_l32) {}
			instr_case_novalue(Op_uitofp_l32_l32) {}
			instr_case_novalue(Op_sitofp_l32_l64) {}
			instr_case_novalue(Op_uitofp_l32_l64) {}

			instr_case_novalue(Op_sitofp_l64_l8) {}
			instr_case_novalue(Op_uitofp_l64_l8) {}
			instr_case_novalue(Op_sitofp_l64_l16) {}
			instr_case_novalue(Op_uitofp_l64_l16) {}
			instr_case_novalue(Op_sitofp_l64_l32) {}
			instr_case_novalue(Op_uitofp_l64_l32) {}
			instr_case_novalue(Op_sitofp_l64_l64) {}
			instr_case_novalue(Op_uitofp_l64_l64) {}

			// Float to Int
			instr_case_novalue(Op_fptosi_l8_l32) {}
			instr_case_novalue(Op_fptoui_l8_l32) {}
			instr_case_novalue(Op_fptosi_l16_l32) {}
			instr_case_novalue(Op_fptoui_l16_l32) {}
			instr_case_novalue(Op_fptosi_l32_l32) {}
			instr_case_novalue(Op_fptoui_l32_l32) {}
			instr_case_novalue(Op_fptosi_l64_l32) {}
			instr_case_novalue(Op_fptoui_l64_l32) {}

			instr_case_novalue(Op_fptosi_l8_l64) {}
			instr_case_novalue(Op_fptoui_l8_l64) {}
			instr_case_novalue(Op_fptosi_l16_l64) {}
			instr_case_novalue(Op_fptoui_l16_l64) {}
			instr_case_novalue(Op_fptosi_l32_l64) {}
			instr_case_novalue(Op_fptoui_l32_l64) {}
			instr_case_novalue(Op_fptosi_l64_l64) {}
			instr_case_novalue(Op_fptoui_l64_l64) {}

			// Float to float
			instr_case_novalue(Op_fpext_l64_l32) {}
			instr_case_novalue(Op_fptrunc_l32_l64) {}

			instr_case_novalue(Op_add_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_l64_imm) {}
			instr_case_novalue(Op_sub_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_l64_imm) {}
			instr_case_novalue(Op_mul_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_l64_imm) {}
			instr_case_novalue(Op_mod_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_l64_imm) {}
			instr_case_novalue(Op_div_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_l64_imm) {}
			instr_case_novalue(Op_neg_l64) {}

			instr_case_novalue(Op_add_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_l32_imm) {}
			instr_case_novalue(Op_sub_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_l32_imm) {}
			instr_case_novalue(Op_mul_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_l32_imm) {}
			instr_case_novalue(Op_mod_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_l32_imm) {}
			instr_case_novalue(Op_div_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_l32_imm) {}
			instr_case_novalue(Op_neg_l32) {}

			instr_case_novalue(Op_add_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_l16_imm) {}
			instr_case_novalue(Op_sub_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_l16_imm) {}
			instr_case_novalue(Op_mul_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_l16_imm) {}
			instr_case_novalue(Op_mod_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_l16_imm) {}
			instr_case_novalue(Op_div_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_l16_imm) {}
			instr_case_novalue(Op_neg_l16) {}

			instr_case_novalue(Op_add_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_l8_imm) {}
			instr_case_novalue(Op_sub_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_l8_imm) {}
			instr_case_novalue(Op_mul_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_l8_imm) {}
			instr_case_novalue(Op_mod_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_l8_imm) {}
			instr_case_novalue(Op_div_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_l8_imm) {}
			instr_case_novalue(Op_neg_l8) {}

			instr_case_novalue(Op_cmpEq_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_l64_imm) {}
			instr_case_novalue(Op_cmpNeq_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_l64_imm) {}
			instr_case_novalue(Op_cmpEq_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_l32_imm) {}
			instr_case_novalue(Op_cmpNeq_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_l32_imm) {}
			instr_case_novalue(Op_cmpEq_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_l16_imm) {}
			instr_case_novalue(Op_cmpNeq_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_l16_imm) {}
			instr_case_novalue(Op_cmpEq_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_l8_imm) {}
			instr_case_novalue(Op_cmpNeq_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_l8_imm) {}

			instr_case_novalue(Op_cmpGt_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_l64_imm) {}
			instr_case_novalue(Op_cmpGe_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_l64_imm) {}
			instr_case_novalue(Op_cmpGt_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_l32_imm) {}
			instr_case_novalue(Op_cmpGe_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_l32_imm) {}
			instr_case_novalue(Op_cmpGt_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_l16_imm) {}
			instr_case_novalue(Op_cmpGe_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_l16_imm) {}
			instr_case_novalue(Op_cmpGt_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_l8_imm) {}
			instr_case_novalue(Op_cmpGe_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_l8_imm) {}

			instr_case_novalue(Op_ucmpGt_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_l64_imm) {}
			instr_case_novalue(Op_ucmpGe_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_l64_imm) {}
			instr_case_novalue(Op_ucmpGt_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_l32_imm) {}
			instr_case_novalue(Op_ucmpGe_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_l32_imm) {}
			instr_case_novalue(Op_ucmpGt_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_l16_imm) {}
			instr_case_novalue(Op_ucmpGe_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_l16_imm) {}
			instr_case_novalue(Op_ucmpGt_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_l8_imm) {}
			instr_case_novalue(Op_ucmpGe_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_l8_imm) {}

			instr_case_novalue(Op_cmpLt_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_l64_imm) {}
			instr_case_novalue(Op_cmpLe_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_l64_imm) {}
			instr_case_novalue(Op_cmpLt_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_l32_imm) {}
			instr_case_novalue(Op_cmpLe_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_l32_imm) {}
			instr_case_novalue(Op_cmpLt_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_l16_imm) {}
			instr_case_novalue(Op_cmpLe_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_l16_imm) {}
			instr_case_novalue(Op_cmpLt_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_l8_imm) {}
			instr_case_novalue(Op_cmpLe_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_l8_imm) {}

			instr_case_novalue(Op_ucmpLt_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_l64_imm) {}
			instr_case_novalue(Op_ucmpLe_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_l64_imm) {}
			instr_case_novalue(Op_ucmpLt_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_l32_imm) {}
			instr_case_novalue(Op_ucmpLe_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_l32_imm) {}
			instr_case_novalue(Op_ucmpLt_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_l16_imm) {}
			instr_case_novalue(Op_ucmpLe_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_l16_imm) {}
			instr_case_novalue(Op_ucmpLt_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_l8_imm) {}
			instr_case_novalue(Op_ucmpLe_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_l8_imm) {}

			instr_case_novalue(Op_fcmpEq_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpEq_l64_imm) {}
			instr_case_novalue(Op_fcmpNeq_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpNeq_l64_imm) {}
			instr_case_novalue(Op_fcmpGt_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGt_l64_imm) {}
			instr_case_novalue(Op_fcmpGe_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGe_l64_imm) {}
			instr_case_novalue(Op_fcmpLt_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLt_l64_imm) {}
			instr_case_novalue(Op_fcmpLe_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLe_l64_imm) {}

			instr_case_novalue(Op_fcmpEq_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpEq_l32_imm) {}
			instr_case_novalue(Op_fcmpNeq_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpNeq_l32_imm) {}
			instr_case_novalue(Op_fcmpGt_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGt_l32_imm) {}
			instr_case_novalue(Op_fcmpGe_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGe_l32_imm) {}
			instr_case_novalue(Op_fcmpLt_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLt_l32_imm) {}
			instr_case_novalue(Op_fcmpLe_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLe_l32_imm) {}

			instr_case_novalue(Op_cmpNull_lptr) {}

			instr_case_novalue(Op_fadd_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fadd_l64_imm) {}
			instr_case_novalue(Op_fadd_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fadd_l32_imm) {}

			instr_case_novalue(Op_fsub_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fsub_l64_imm) {}
			instr_case_novalue(Op_fsub_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fsub_l32_imm) {}

			instr_case_novalue(Op_fmul_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fmul_l64_imm) {}
			instr_case_novalue(Op_fmul_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fmul_l32_imm) {}

			instr_case_novalue(Op_fdiv_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fdiv_l64_imm) {}
			instr_case_novalue(Op_fdiv_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fdiv_l32_imm) {}

			instr_case_novalue(Op_fneg_l64) {}
			instr_case_novalue(Op_fneg_l32) {}

			instr_case_novalue(Op_umul_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_l64_imm) {}
			instr_case_novalue(Op_umod_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_l64_imm) {}
			instr_case_novalue(Op_udiv_l64_l64) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_l64_imm) {}

			instr_case_novalue(Op_umul_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_l32_imm) {}
			instr_case_novalue(Op_umod_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_l32_imm) {}
			instr_case_novalue(Op_udiv_l32_l32) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_l32_imm) {}

			instr_case_novalue(Op_umul_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_l16_imm) {}
			instr_case_novalue(Op_umod_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_l16_imm) {}
			instr_case_novalue(Op_udiv_l16_l16) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_l16_imm) {}

			instr_case_novalue(Op_umul_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_l8_imm) {}
			instr_case_novalue(Op_umod_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_l8_imm) {}
			instr_case_novalue(Op_udiv_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_l8_imm) {}

			instr_case_novalue(Op_log_and_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_log_and_l8_imm) {}
			instr_case_novalue(Op_log_or_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_log_or_l8_imm) {}
			instr_case_novalue(Op_log_xor_l8_l8) {
				validateStackPrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_log_xor_l8_imm) {}
			instr_case_novalue(Op_log_not_l8) {}


			instr_case(Op_variantSetInner_lvnt_type, instr) {
				const auto variant_type = current_stack.at(instr.variant.var_name)
				                              ->getKindAs<valid_type::finalized::Variant>();
				valid_type::ValidTypeID wanted_type
					= types_ctx.at(instr.inner_type.type_name)->getID();

				if (!variant_type->alternatives_set.contains(wanted_type))
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantGetInner_lptr_lvnt_type, instr) {
				const auto variant_type = current_stack.at(instr.variant.var_name)
				                              ->getKindAs<valid_type::finalized::Variant>();
				const auto pointer_type = current_stack.at(instr.dst_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				auto wanted_type = types_ctx.at(pointer_type->inner);
				if (!variant_type->alternatives_set.contains(wanted_type->getID()))
					throw VariantTypeMismatchError(instr);

				if (instr.expected_type != wanted_type->getName())
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantSetInner_lptr_type, instr) {
				const auto variant_pointer = current_stack.at(instr.variant_ptr.var_name)
				                                 ->getKindAs<valid_type::finalized::Pointer>();

				const auto variant_type = expectPointerType<valid_type::finalized::Variant>(
					variant_pointer, types_ctx, instr
				);

				auto wanted_type = types_ctx.at(instr.inner_type.type_name);
				if (!variant_type->alternatives_set.contains(wanted_type->getID()))
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantGetInner_lptr_lptr_type, instr) {
				const auto pointer_type = current_stack.at(instr.dst_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				auto wanted_type = types_ctx.at(pointer_type->inner);


				const auto variant_pointer = current_stack.at(instr.variant_ptr.var_name)
				                                 ->getKindAs<valid_type::finalized::Pointer>();
				const auto variant_type = expectPointerType<valid_type::finalized::Variant>(
					variant_pointer, types_ctx, instr
				);

				if (!variant_type->alternatives_set.contains(wanted_type->getID()))
					throw VariantTypeMismatchError(instr);

				if (instr.expected_type != wanted_type->getName())
					throw VariantTypeMismatchError(instr);
			}
			instr_case_novalue(Op_label) {}
			instr_case_novalue(Op_jmp_label) {}
			instr_case_novalue(Op_jmpIf_label) {}
			instr_case_novalue(Op_jmpIfNot_label) {}
			instr_case_novalue(Op_call_func) {}
			instr_case_novalue(Op_call_builtinfunc) {}
			instr_case_novalue(Op_call_cfunc) {}
			instr_case_novalue(Op_set_threadctx) {}
			instr_case(Op_virtual_call_lptr_method, instr) {
				// For a method call to be valid it has to be present in the interface.
				const auto pointer_type = current_stack.at(instr.object_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				const auto& inner_pointer_type = types_ctx.at(pointer_type->inner);
				const auto  obj_type
					= inner_pointer_type->maybeGetKindAs<valid_type::finalized::Structure>()
				          .expect<InvalidVirtualCallError>(instr);
				const auto& inh_meta
					= obj_type->inheritance_metadata.expect<InvalidVirtualCallError>(instr);

				if (!inh_meta.available_methods.contains(instr.method.method_name))
					throw InvalidVirtualCallError(instr);
			}
			instr_case_novalue(Op_ret_tailcall_func) {}
			instr_case_novalue(Op_ret) {}
			instr_case_novalue(Op_deinit) {}
			instr_case_novalue(Op_input_l64) {}
			instr_case_novalue(Op_output_l64) {}
			instr_case_novalue(Op_input_l32) {}
			instr_case_novalue(Op_output_l32) {}
			instr_case(Op_setVTable_lptr_type, instr) {
				const auto pointer_type = current_stack.at(instr.object_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				if (pointer_type->inner != types_ctx.at(instr.type.type_name)->getID())
					throw VTableTypeMismatchError(instr);
			}
			instr_case(Op_resetVTable_lptr, instr) {
				const auto pointer_type = current_stack.at(instr.object_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				const auto structure = expectPointerType<valid_type::finalized::Structure>(
					pointer_type, types_ctx, instr
				);
				if (!structure->inheritance_metadata) throw NotAClassTypeError(instr);
			}
			instr_case(Op_downcast_lptr_lptr, instr) {
				validateClassCast<InvalidDowncastError>(instr, current_stack);
			}
			instr_case_novalue(Op_free_lptr) {}
			instr_case(Op_store_lptr_lany, instr) {
				const auto pointer_type = current_stack.at(instr.dst_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				CRef<valid_type::ValidType> other_type = current_stack.at(instr.src.var_name);
				if (pointer_type->inner != other_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_load_lany_lptr, instr) {
				const auto pointer_type = current_stack.at(instr.src_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				CRef<valid_type::ValidType> other_type = current_stack.at(instr.dst.var_name);
				if (pointer_type->inner != other_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_ref_lptr_lany, instr) {
				const auto pointer_type = current_stack.at(instr.dst_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				CRef<valid_type::ValidType> other_type = current_stack.at(instr.src.var_name);
				if (pointer_type->inner != other_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_ref_lptr_gany, instr) {
				const auto pointer_type = current_stack.at(instr.dst_ptr.var_name)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				const auto&                 global_entry = globals.at(instr.src.global_data_name);
				CRef<valid_type::ValidType> global_type  = types_ctx.at(global_entry->type);
				if (pointer_type->inner != global_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_structLea_lptr_lptr_field, instr) {
				const auto destination = current_stack.at(instr.dst_ptr.var_name)
				                             ->getKindAs<valid_type::finalized::Pointer>();

				const auto ztruct_pointer = current_stack.at(instr.src_data_ptr.var_name)
				                                ->getKindAs<valid_type::finalized::Pointer>();
				const auto ztruct = expectPointerType<valid_type::finalized::Structure>(
					ztruct_pointer, types_ctx, instr
				);

				validateStructFieldType(
					*types_ctx.at(ztruct_pointer->inner),
					*ztruct,
					instr.field,
					destination->inner,
					instr
				);
			}
			instr_case(Op_structLoad_lany_lptr_field, instr) {
				const auto& destination = current_stack.at(instr.dst.var_name);

				const auto ztruct_pointer = current_stack.at(instr.src_data_ptr.var_name)
				                                ->getKindAs<valid_type::finalized::Pointer>();
				const auto ztruct = expectPointerType<valid_type::finalized::Structure>(
					ztruct_pointer, types_ctx, instr
				);

				validateStructFieldType(
					*types_ctx.at(ztruct_pointer->inner),
					*ztruct,
					instr.field,
					destination->getID(),
					instr
				);
			}
			instr_case(Op_structStore_lptr_lany_field, instr) {
				const auto& source = current_stack.at(instr.src.var_name);

				const auto ztruct_pointer = current_stack.at(instr.dst_data_ptr.var_name)
				                                ->getKindAs<valid_type::finalized::Pointer>();
				const auto ztruct = expectPointerType<valid_type::finalized::Structure>(
					ztruct_pointer, types_ctx, instr
				);

				validateStructFieldType(
					*types_ctx.at(ztruct_pointer->inner), *ztruct, instr.field, source->getID(), instr
				);
			}

			instr_case(Op_structLea_lptr_lste_field, instr) {
				auto dst = current_stack.at(instr.dst_ptr.var_name)
				               ->getKindAs<valid_type::finalized::Pointer>();
				auto src        = current_stack.at(instr.src_data_struct.var_name);
				auto src_struct = src->getKindAs<valid_type::finalized::Structure>();
				validateStructFieldType(*src, *src_struct, instr.field, dst->inner, instr);
			}
			instr_case(Op_structLoad_lany_lste_field, instr) {
				auto target_type = types_ctx.at(current_stack.at(instr.dst.var_name)->getID());
				auto source_type
					= types_ctx.at(current_stack.at(instr.src_data_struct.var_name)->getID());
				validateStructFieldType(
					*source_type,
					*source_type->getKindAs<valid_type::finalized::Structure>(),
					instr.field,
					target_type->getID(),
					instr
				);
			}
			instr_case(Op_structStore_lste_lany_field, instr) {
				auto target_type
					= types_ctx.at(current_stack.at(instr.dst_data_struct.var_name)->getID());
				auto source_type = types_ctx.at(current_stack.at(instr.src.var_name)->getID());
				validateStructFieldType(
					*target_type,
					*target_type->getKindAs<valid_type::finalized::Structure>(),
					instr.field,
					source_type->getID(),
					instr
				);
			}

			instr_case(Op_fixedSizeTableLea_lptr_lptr_l64, instr) {
				const auto destination = current_stack.at(instr.dst_ptr.var_name)
				                             ->getKindAs<valid_type::finalized::Pointer>();

				const auto table_pointer = current_stack.at(instr.src_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::FixedSizeTable>(
					table_pointer, types_ctx, instr
				);

				if (destination->inner != table_type->inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableLea_lptr_lptr_l64, instr) {
				const auto destination = current_stack.at(instr.dst_ptr.var_name)
				                             ->getKindAs<valid_type::finalized::Pointer>();

				const auto table_pointer = current_stack.at(instr.src_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);

				if (destination->inner != table_type->inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableLoad_lany_lptr_l64, instr) {
				const auto& destination = current_stack.at(instr.dst.var_name);

				const auto table_pointer = current_stack.at(instr.src_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::FixedSizeTable>(
					table_pointer, types_ctx, instr
				);

				if (destination->getID() != table_type->inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableLoad_lany_lptr_l64, instr) {
				const auto& destination   = current_stack.at(instr.dst.var_name);
				const auto  table_pointer = current_stack.at(instr.src_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);
				if (destination->getID() != table_type->inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableStore_lptr_lany_l64, instr) {
				const auto& source = current_stack.at(instr.src.var_name);

				const auto table_pointer = current_stack.at(instr.dst_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::FixedSizeTable>(
					table_pointer, types_ctx, instr
				);

				if (table_type->inner != source->getID())
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableStore_lptr_lany_l64, instr) {
				const auto& source        = current_stack.at(instr.src.var_name);
				const auto  table_pointer = current_stack.at(instr.dst_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);

				if (table_type->inner != source->getID())
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableReAlloc_lptr_type_l64, instr) {
				const auto table_pointer = current_stack.at(instr.dst_table_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);

				valid_type::ValidTypeID allocated_type
					= types_ctx.at(instr.table_type.type_name)->getID();
				if (allocated_type != table_pointer->inner)
					throw InvalidArgumentTypeError(instr.table_type);
			}
			instr_case(Op_strOutput_lptr, instr) {
				const auto table_pointer = current_stack.at(instr.string_ptr.var_name)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);
				if (types_ctx.at(table_type->inner)->getName() != "byte")
					throw DynamicTableTypeMismatchError(instr);
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
		auto type = types_ctx.at(arg.type_name);
		if (!type->isInstantiable()) throw UninstantiableValueError(arg);
	}

	template<class Error, class Instr>
	requires(std::is_same_v<Instr, std::remove_cvref_t<Op_upcast_lptr_lptr>> || std::is_same_v<Instr, std::remove_cvref_t<Op_downcast_lptr_lptr>>)
	void validateClassCast(const Instr& instruction, const LocalStack& current_stack) const {
		auto higher_ptr_tod = current_stack.at(instruction.dst.var_name);
		auto lower_ptr_tod  = current_stack.at(instruction.src.var_name);
		if constexpr (std::is_same_v<Instr, std::remove_cvref_t<Op_downcast_lptr_lptr>>)
			std::swap(higher_ptr_tod, lower_ptr_tod);

		// Higher or lower in terms of inheritance hierarchy tree, base/superclass is "higher".
		auto higher_type = types_ctx.at(
			higher_ptr_tod->template getKindAs<valid_type::finalized::Pointer>()->inner
		);
		auto lower_type = types_ctx.at(
			lower_ptr_tod->template getKindAs<valid_type::finalized::Pointer>()->inner
		);

		const bool inherits
			= lower_type->template maybeGetKindAs<valid_type::finalized::Structure>()
		          .flatMap([](CRef<valid_type::finalized::Structure> lower_struct) {
					  return lower_struct->inheritance_metadata;
				  })
		          .map([higher_type](const valid_type::finalized::InheritanceMetadata& lower_imd) {
					  return lower_imd.super_types.contains(higher_type->getID());
				  })
		          .copyValueOr(false);

		if (!inherits) throw Error(instruction);
	}

	void validatePrimitiveCast(
		const opargs::OpCodePrimitiveArg& local,
		const opargs::Type&               type,
		const Instruction&                instruction,
		const LocalStack&                 current_stack
	) const {
		// These are guaranteed to exist by `validateArgTypes`.
		const auto curr_type      = current_stack.at(VISIT(local, l, return l.var_name));
		const auto new_type       = types_ctx.at(type.type_name);
		const auto curr_primitive = curr_type->getKindAs<valid_type::finalized::Primitive>();

		const auto new_primitive = new_type->maybeGetKindAs<valid_type::finalized::Primitive>()
		                               .expect<NonPrimitiveCastError>(type);
		if (curr_primitive->size != new_primitive->size) throw CastSizeMismatchError(instruction);
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
		LocalStack local_stack(function.signature, types_ctx);
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
		if (function.name.str == base::StrID("main")) {
			if (function.signature.result_types.size() != 1)
				throw InvalidMainReturnType(function.signature, 1);
			if (function.signature.result_types[0].str != base::StrID("i64"))
				throw InvalidMainReturnType(function.signature, 0);
		}
		for (const auto& param_type: function.signature.parameters)
			if (!types_ctx.contains(param_type)) throw UnknownTypeError(opargs::Type{ param_type });

		for (const auto& reslts: function.signature.result_types)
			if (!types_ctx.contains(reslts)) throw UnknownTypeError(opargs::Type{ reslts });
	}

public:
	FunctionValidator(
		const valid_type::ValidTypeMap&                  types_ctx,
		const ObjIdNameMap<GlobalData>&                  globals,
		const base::HashMap<base::StrID, FuncSignature>& signatures,
		const ObjIdNameMap<ExternalCFunction>&           ext_c_signatures,
		const Function&                                  function
	):
		  types_ctx(types_ctx),
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
	const valid_type::ValidTypeMap&                  types,
	const ObjIdNameMap<GlobalData>&                  globals_map,
	const base::HashMap<base::StrID, FuncSignature>& signatures,
	const ObjIdNameMap<ExternalCFunction>&           ext_c_signatures,
	const Function&                                  function
) {
	FuncSignature signature = signatures.at(function.name);


	FunctionValidator validator(types, globals_map, signatures, ext_c_signatures, function);

	Function new_function;
	new_function.name         = function.name;
	new_function.body         = validator.validateAndExtractReachableCode();
	new_function.bytecode_pos = function.bytecode_pos;
	new_function.signature    = signature;

	return new_function;
}
