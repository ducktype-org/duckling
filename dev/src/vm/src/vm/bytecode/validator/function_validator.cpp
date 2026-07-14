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
#include <vm/bytecode/validator/local_stack_database_builder.hpp>
#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/builtin_functions.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <ranges>
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
		Op_call_ffifunc,
		Op_virtual_call_pptr_method>;
	using CallingInstructions
		= std::tuple<Op_call_func, Op_call_builtinfunc, Op_call_cfunc, Op_call_ffifunc>;

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

/**
 * @brief A wrapper class for both factory for local stack and the local stack database itself.
 * @note This is so that it can be used both on completed and currently built local-stack. This
 * way, building a database can be done before, during or after validation.
 */
class LocalStack {
	// the following are CRefs instead of const& to allow copy/move.

	CRef<valid_type::ValidTypeMap>                             types_ctx;
	std::variant<CRef<LocalStackDb>, Ref<LocalStackDbBuilder>> source;
	StackStateID                                               stack_state_id;
	usize                                                      number_of_ret_vals = 0;

public:
	LocalStack(const LocalStack&)            = default;
	LocalStack(LocalStack&&)                 = default;
	LocalStack& operator=(const LocalStack&) = default;
	LocalStack& operator=(LocalStack&&)      = default;

	LocalStack(
		const valid_type::ValidTypeMap& types_ctx,
		Ref<LocalStackDbBuilder>        src,
		StackStateID                    state,
		usize                           number_of_rets
	):
		  types_ctx(&types_ctx),
		  source(src),
		  stack_state_id(state),
		  number_of_ret_vals(number_of_rets) {}

	[[nodiscard]]
	StackStateID getStateID() const {
		return stack_state_id;
	}

	[[nodiscard]]
	std::vector<LocalStackEntry> getStackState() const {
		auto                         vec_size = size();
		std::vector<LocalStackEntry> ans;
		ans.reserve(vec_size);
		for (usize i = 0; i < vec_size; i++) ans.push_back(front(i));
		return ans;
	}

	void push(const opargs::PlaceAny& local, const opargs::Type& type) {
		CORE_ASSERT(
			std::holds_alternative<Ref<LocalStackDbBuilder>>(source),
			"We need to be building local stack database to push variables to stack"
		);

		auto bld_ref = std::get<Ref<LocalStackDbBuilder>>(source);
		if (bld_ref->contains(stack_state_id, local.var_name))
			throw DuplicatedLocalNameError(local);

		auto new_state = bld_ref->push(stack_state_id, local.var_name, type.type_name);
		stack_state_id = new_state;
	}

	/**
	 * @brief Pops the top element from the stack state and updates local variable mappings.
	 * Can be only used with instructions which effectively deinitialize the local stack
	 * (deinit, call_func, virtual_call and call_builtinfunc)
	 */
	template<DeinitializingInstruction InstructionType>
	void pop(const InstructionType& cause) {
		CORE_ASSERT(
			std::holds_alternative<Ref<LocalStackDbBuilder>>(source),
			"We need to be building local stack database to pop variables from stack"
		);

		auto bld_ref = std::get<Ref<LocalStackDbBuilder>>(source);

		auto size = bld_ref->size(stack_state_id);
		if (size == number_of_ret_vals) throw RetValDeinitError(cause);
		// note: we dont't allow to pop the ret-vals from stack, because that would be weird (e.g.
		// caller's variable has not changed the name, but has changed the block...)

		auto new_state = bld_ref->pop(stack_state_id);
		auto new_size  = bld_ref->size(new_state);
		CORE_ASSERT(new_size + 1 == size, "we expect that the size must be valid");
		stack_state_id = new_state;
	}

	[[nodiscard]]
	usize size() const {
		return VISIT(source, db, return db->size(stack_state_id));
	}

	[[nodiscard]]
	LocalStackEntry back(usize i = 0) const {
		usize end = size();
		CORE_ASSERT(i < end, "we want idx to be smaller than size");
		return front(end - 1 - i);
	}

	[[nodiscard]]
	LocalStackEntry front(usize idx = 0) const {
		return VISIT(
			source,
			db,
			return LocalStackEntry{
				.local_name = *db->getName(stack_state_id, idx),
				.type       = types_ctx->at(*db->getTypeName(stack_state_id, idx)),
			}
		);
	}

	void castPrimitive(const opargs::OpCodePrimitiveArg& local, const opargs::Type& type) {
		CORE_ASSERT(
			std::holds_alternative<Ref<LocalStackDbBuilder>>(source),
			"We need to be building local stack database to cast type of variables"
		);

		auto bld_ref = std::get<Ref<LocalStackDbBuilder>>(source);

		auto local_name = VISIT(local, l, return l.var_name);
		auto new_state  = bld_ref->change(stack_state_id, local_name, type.type_name);
		stack_state_id  = new_state;
	}

	[[nodiscard]]
	bool contains(base::StrID local_name) const {
		return VISIT(source, db, return db->contains(stack_state_id, local_name););
	}

	[[nodiscard]]
	CRef<valid_type::ValidType> at(base::StrID local_name) const {
		return VISIT(source, db, return types_ctx->at(*db->getTypeName(stack_state_id, local_name)));
	}

	/**
	 * @brief Function for determining if stack is the same at two states (== operator).
	 * @warning THIS FUNCTION IS O(n) AS OF 27.05.2026 - Use it with care or upload a persistant
	 * structures library
	 */
	[[nodiscard]]
	bool eqStack(StackStateID stack_state_1, StackStateID stack_state_2) const {
		return VISIT(
			source,
			db,
			return db->eqTypes(stack_state_1, stack_state_2)
		        && db->eqNames(stack_state_1, stack_state_2)
		);
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
	const ObjIdNameMap<FFIFunction>&                 ffi_signatures;
	const Function&                                  function;

	std::vector<base::Optional<StackStateID>>            stack_before_instr;
	base::HashMap<base::StrID, usize>                    index_of_label;
	base::HashMap<base::StrID, std::vector<Instruction>> jumps_to_label;

	template<CallingInstruction CallInstructionType>
	void validateCallAndPop(LocalStack& local_stack, const CallInstructionType& instr) {
		// Used for errors.
		auto generic_arg = opargs::OpCodeArg{ instr.function };

		CRef<FuncSignature> signature = [&] -> CRef<FuncSignature> {
			if constexpr (std::is_same_v<opargs::BuiltinFunctionName, decltype(instr.function)>)
				return *builtins::getBuiltinFunctionSignature(instr.function.function_name);
			if constexpr (std::is_same_v<opargs::ExtCFunctionName, decltype(instr.function)>)
				return &ext_c_signatures.at(instr.function.function_name)->signature;
			if constexpr (std::is_same_v<opargs::FFIFunctionName, decltype(instr.function)>)
				return &ffi_signatures.at(instr.function.function_name)->signature;
			return &signatures.at(instr.function.function_name);
		}();

		auto& params  = signature->parameters;
		auto& results = signature->result_types;

		if (params.size() + results.size() > local_stack.size())
			throw InvalidFunctionCallArgumentsError(generic_arg);

		using namespace std::views;
		for (auto param: params | reverse) {
			if (local_stack.back().type->getName() != param.str)
				throw InvalidFunctionCallArgumentsError(generic_arg);
			local_stack.pop(instr);
		}

		for (auto [idx, reslt]: enumerate(results | reverse))
			if (local_stack.back(usize(idx)).type->getName() != reslt.str)
				throw InvalidFunctionCallArgumentsError(generic_arg);
	}

	/**
	 * @brief Validates the stack state at the moment of a method call.
	 *
	 * @note Method calls need their own handling.
	 * - The argument is only the name of the method and we need to find an implementation
	 *   corresponding to that name.
	 * - The first argument on the stack should be pointer which points to the same type as the
	 *   pointer passed as the `obj_ptr` (an argument to `virtual_call_pptr_method`).
	 *   Since virtual_method map contains only the signatures of methods, the implementations of
	 *   them may declare a pointer to a different type (only a pointer to SELF - subclass can
	 * differ). Validating just the pointer type name like in normal function calls would simply
	 * don't work.
	 */
	void validateMethodCallAndPop(LocalStack& local_stack, const Op_virtual_call_pptr_method& instr) {
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

		for (auto [idx, reslt]: zip(iota(0u), reslts | reverse))
			if (local_stack.back(idx).type->getID() != reslt)
				throw InvalidFunctionCallArgumentsError(generic_arg);
	}

	void validateRet(
		const LocalStack& local_stack, const Op_ret& instr, const FuncSignature& current_signature
	) {
		auto& returns    = current_signature.result_types;
		usize ret_amount = returns.size();
		CORE_ASSERT(
			local_stack.size() >= ret_amount,
			"Since we cannot pop the ret-vals, all the original return values must be on the stack"
		);

		// note: As of 22-06-2026, the only way for the function to return invalid types is via
		// incorrect casting

		for (usize i = 0; i < ret_amount; i++)
			if (local_stack.front(i).type->getName() != returns.at(i).str)
				throw InvalidRetError(instr);
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

	template<typename PlaceT>
	CRef<valid_type::ValidType> validateAndGetPlaceType(
		const PlaceT& place, const LocalStack& current_stack
	) const {
		bool is_local  = current_stack.contains(place.var_name);
		bool is_global = globals.contains(place.var_name);
		if (is_local && is_global) throw DuplicatedLocalNameError(place);
		if (!is_local && !is_global) throw UnknownLocalNameError(place);
		return is_local ? current_stack.at(place.var_name)
		                : types_ctx.at(globals.at(place.var_name)->type);
	}

	template<typename PlaceT>
	CRef<valid_type::ValidType> getPlaceType(const PlaceT& place, const LocalStack& current_stack)
		const {
		bool is_local = current_stack.contains(place.var_name);
		return is_local ? current_stack.at(place.var_name)
		                : types_ctx.at(globals.at(place.var_name)->type);
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
#define PLACE_CASE(BIT_COUNT)                                                              \
	variant_case(CRef<opargs::Place##BIT_COUNT>, place) {                                  \
		CRef<valid_type::ValidType> type = validateAndGetPlaceType(*place, current_stack); \
		variant_match(type->getKind()) {                                                   \
			variant_case(valid_type::finalized::Primitive, primitive_type) {               \
				if (base::bytes2bits(primitive_type.size).asInt() != BIT_COUNT)            \
					throw InvalidArgumentSizeError(*place);                                \
			}                                                                              \
			variant_default { throw InvalidArgumentTypeError(*place); }                    \
		}                                                                                  \
	}
				PLACE_CASE(8);
				PLACE_CASE(16);
				PLACE_CASE(32);
				PLACE_CASE(64);

				variant_case(CRef<opargs::PlacePtr>, place) {
					CRef<valid_type::ValidType> type
						= validateAndGetPlaceType(*place, current_stack);
					if (!type->isKind<valid_type::finalized::Pointer>())
						throw InvalidArgumentTypeError(*place);
				}
				variant_case(CRef<opargs::PlaceAny>, place) {
					bool is_local  = current_stack.contains(place->var_name);
					bool is_global = globals.contains(place->var_name);
					if (is_local && is_global) throw DuplicatedLocalNameError(*place);
					instr_match(instruction) {
						instr_case_novalue(Op_init_pany_type) {
							if (is_local || is_global) throw DuplicatedLocalNameError(*place);
						}
						variant_default {
							if (!is_local && !is_global) throw UnknownLocalNameError(*place);
						}
					}
				}
				variant_case(CRef<opargs::PlaceOpq>, place) {
					CRef<valid_type::ValidType> type
						= validateAndGetPlaceType(*place, current_stack);
					if (!type->isKind<valid_type::finalized::Opaque>())
						throw InvalidArgumentTypeError(*place);
				}
				variant_case(CRef<opargs::PlaceCptr>, place) {
					CRef<valid_type::ValidType> type
						= validateAndGetPlaceType(*place, current_stack);
					if (!type->isKind<valid_type::finalized::CPointer>())
						throw InvalidArgumentTypeError(*place);
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
				variant_case(CRef<opargs::FFIFunctionName>, function_value) {
					auto fun_name = function_value->function_name;
					if (!ffi_signatures.contains(fun_name))
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

				variant_case(CRef<opargs::PlaceVnt>, place) {
					CRef<valid_type::ValidType> type
						= validateAndGetPlaceType(*place, current_stack);
					if (!type->isKind<valid_type::finalized::Variant>())
						throw InvalidArgumentTypeError(*place);
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

				variant_case(CRef<opargs::PlaceStructure>, place) {
					CRef<valid_type::ValidType> type
						= validateAndGetPlaceType(*place, current_stack);
					if (!type->isKind<valid_type::finalized::Structure>())
						throw InvalidArgumentTypeError(*place);
				}

				variant_case(CRef<opargs::PlaceFSTable>, place) {
					CRef<valid_type::ValidType> type
						= validateAndGetPlaceType(*place, current_stack);
					if (!type->isKind<valid_type::finalized::FixedSizeTable>())
						throw InvalidArgumentTypeError(*place);
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
	 * for instructions which require this, e.g. `mov_p8_p8` - both arguments must be of the same
	 * primitive type - Invalid combination is (e.g. `i32` and `u32`).
	 * @param instruction Instruction that is validated.
	 */
	void validatePlacePrimitiveArgumentsSameType(
		const Instruction& instruction, const LocalStack& current_stack
	) const {
		std::vector<valid_type::ValidTypeID> primitive_args;

		for (auto arg: instruction.args()) {
			variant_match(arg) {
#define PLACE_CASE_PRIMITIVE_VALIDATION(BIT_COUNT)                              \
	variant_case(CRef<opargs::Place##BIT_COUNT>, place) {                       \
		primitive_args.push_back(getPlaceType(*place, current_stack)->getID()); \
	}

				FOR_EACH(PLACE_CASE_PRIMITIVE_VALIDATION, 8, 16, 32, 64)
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
			instr_case(Op_init_pany_type, instr) { validateArgInstantiable(instr.type); }
			instr_case(Op_alloc_pptr_type, instr) {
				validateArgInstantiable(instr.type);
				CRef<valid_type::ValidType> variable = getPlaceType(instr.ptr, current_stack);
				CRef<valid_type::finalized::Pointer> pointer
					= variable->getKindAs<valid_type::finalized::Pointer>();
				if (types_ctx.at(pointer->inner)->getName() != instr.type.type_name)
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_upcast_pptr_pptr, instr) {
				validateClassCast<InvalidUpcastError>(instr, current_stack);
			}
			instr_case(Op_cast_p8_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case(Op_cast_p16_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case(Op_cast_p32_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case(Op_cast_p64_type, instr) {
				validatePrimitiveCast(instr.value, instr.target_type, instruction, current_stack);
			}
			instr_case_novalue(Comment) {}
			instr_case_novalue(Op_mov_p8_imm) {}
			instr_case(Op_mov_p8_p8, instr) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case(Op_cmov_p8_p8, instr) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p8_imm, Op_mov_p16_imm) {}
			instr_case_novalue(Op_mov_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p16_imm, Op_mov_p32_imm) {}
			instr_case_novalue(Op_mov_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p32_imm, Op_mov_p64_imm) {}
			instr_case_novalue(Op_mov_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmov_p64_imm) {}

			instr_case(Op_mov_pptr_pptr, instr) {
				auto src_type = getPlaceType(instr.src, current_stack)->getName();
				auto dst_type = getPlaceType(instr.dst, current_stack)->getName();
				if (src_type != dst_type) throw PointerTypeMismatchError(instr);
			}

			instr_case_novalue(Op_mov_popq_popq) {}

			instr_case(Op_mov_pcpt_pcpt, instr) {
				// Strict: no implicit pointee change on copy; reinterpretation goes through the
				// explicit `cptrCast_pcpt_pcpt`.
				if (getPlaceType(instr.src, current_stack)->getID()
				    != getPlaceType(instr.dst, current_stack)->getID())
					throw CPointerTypeMismatchError(instr);
			}

			instr_case(Op_cptrLoad_pany_pcpt, instr) {
				const auto cpointer = getPlaceType(instr.src_ptr, current_stack)
				                          ->getKindAs<valid_type::finalized::CPointer>();
				// Dereferencing needs a known pointee whose VM layout matches the native one.
				if (!cpointer->inner.has_value()
				    || !types_ctx.at(*cpointer->inner)->isFFICompliant())
					throw CPtrNotDereferenceableError(instr);
				if (getPlaceType(instr.dst, current_stack)->getID() != *cpointer->inner)
					throw CPtrPointeeMismatchError(instr);
			}
			instr_case(Op_cptrStore_pcpt_pany, instr) {
				const auto cpointer = getPlaceType(instr.dst_ptr, current_stack)
				                          ->getKindAs<valid_type::finalized::CPointer>();
				if (!cpointer->inner.has_value()
				    || !types_ctx.at(*cpointer->inner)->isFFICompliant())
					throw CPtrNotDereferenceableError(instr);
				if (getPlaceType(instr.src, current_stack)->getID() != *cpointer->inner)
					throw CPtrPointeeMismatchError(instr);
			}
			// The raw byte copies work through any cpointer (the VM side is bounds-checked at
			// runtime), but the VM-side pointee must be trivially copyable, so raw native bytes
			// never overwrite (or leak) VM-managed data.
			instr_case(Op_cptrRead_pptr_pcpt_p64, instr) {
				const auto pointer = getPlaceType(instr.dst_ptr, current_stack)
				                         ->getKindAs<valid_type::finalized::Pointer>();
				if (!types_ctx.at(pointer->inner)->isTriviallyCopyable())
					throw CPtrRawCopyPointeeError(instr);
			}
			instr_case(Op_cptrWrite_pcpt_pptr_p64, instr) {
				const auto pointer = getPlaceType(instr.src_ptr, current_stack)
				                         ->getKindAs<valid_type::finalized::Pointer>();
				if (!types_ctx.at(pointer->inner)->isTriviallyCopyable())
					throw CPtrRawCopyPointeeError(instr);
			}
			// A cast converts any cpointer to any other - the generic argument validation
			// already pins the operand kinds.
			instr_case_novalue(Op_cptrCast_pcpt_pcpt) {}
			instr_case(Op_cptrAddOffset_pcpt_pcpt_p64, instr) {
				if (getPlaceType(instr.src, current_stack)->getID()
				    != getPlaceType(instr.dst, current_stack)->getID())
					throw CPointerTypeMismatchError(instr);
			}
			// A null check works through any cpointer type.
			instr_case_novalue(Op_cmpNull_pcpt) {}

			instr_case(Op_mov_pste_pste, instr) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case(Op_mov_pfst_pfst, instr) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}

			instr_case_novalue(Op_setNull_pptr) {}

			// Sign Extension
			instr_case_novalue(
				Op_sext_p16_p8,
				Op_sext_p32_p8,
				Op_sext_p64_p8,
				Op_sext_p32_p16,
				Op_sext_p64_p16,
				Op_sext_p64_p32
			) {}

			// Zero Extension
			instr_case_novalue(
				Op_zext_p16_p8,
				Op_zext_p32_p8,
				Op_zext_p64_p8,
				Op_zext_p32_p16,
				Op_zext_p64_p16,
				Op_zext_p64_p32
			) {}

			// Truncation
			instr_case_novalue(
				Op_trunc_p8_p16,
				Op_trunc_p8_p32,
				Op_trunc_p8_p64,
				Op_trunc_p16_p32,
				Op_trunc_p16_p64,
				Op_trunc_p32_p64
			) {}

			// Int to Float
			instr_case_novalue(
				Op_sitofp_p32_p8,
				Op_uitofp_p32_p8,
				Op_sitofp_p32_p16,
				Op_uitofp_p32_p16,
				Op_sitofp_p32_p32,
				Op_uitofp_p32_p32,
				Op_sitofp_p32_p64,
				Op_uitofp_p32_p64
			) {}

			instr_case_novalue(
				Op_sitofp_p64_p8,
				Op_uitofp_p64_p8,
				Op_sitofp_p64_p16,
				Op_uitofp_p64_p16,
				Op_sitofp_p64_p32,
				Op_uitofp_p64_p32,
				Op_sitofp_p64_p64,
				Op_uitofp_p64_p64
			) {}

			// Float to Int
			instr_case_novalue(
				Op_fptosi_p8_p32,
				Op_fptoui_p8_p32,
				Op_fptosi_p16_p32,
				Op_fptoui_p16_p32,
				Op_fptosi_p32_p32,
				Op_fptoui_p32_p32,
				Op_fptosi_p64_p32,
				Op_fptoui_p64_p32
			) {}

			instr_case_novalue(
				Op_fptosi_p8_p64,
				Op_fptoui_p8_p64,
				Op_fptosi_p16_p64,
				Op_fptoui_p16_p64,
				Op_fptosi_p32_p64,
				Op_fptoui_p32_p64,
				Op_fptosi_p64_p64,
				Op_fptoui_p64_p64
			) {}

			// Float to float
			instr_case_novalue(Op_fpext_p64_p32, Op_fptrunc_p32_p64) {}

			instr_case_novalue(Op_add_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_p64_imm) {}
			instr_case_novalue(Op_sub_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_p64_imm) {}
			instr_case_novalue(Op_mul_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_p64_imm) {}
			instr_case_novalue(Op_mod_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_p64_imm) {}
			instr_case_novalue(Op_div_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_p64_imm) {}
			instr_case_novalue(Op_neg_p64) {}

			instr_case_novalue(Op_add_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_p32_imm) {}
			instr_case_novalue(Op_sub_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_p32_imm) {}
			instr_case_novalue(Op_mul_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_p32_imm) {}
			instr_case_novalue(Op_mod_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_p32_imm) {}
			instr_case_novalue(Op_div_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_p32_imm) {}
			instr_case_novalue(Op_neg_p32) {}

			instr_case_novalue(Op_add_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_p16_imm) {}
			instr_case_novalue(Op_sub_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_p16_imm) {}
			instr_case_novalue(Op_mul_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_p16_imm) {}
			instr_case_novalue(Op_mod_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_p16_imm) {}
			instr_case_novalue(Op_div_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_p16_imm) {}
			instr_case_novalue(Op_neg_p16) {}

			instr_case_novalue(Op_add_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_add_p8_imm) {}
			instr_case_novalue(Op_sub_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_sub_p8_imm) {}
			instr_case_novalue(Op_mul_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mul_p8_imm) {}
			instr_case_novalue(Op_mod_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_mod_p8_imm) {}
			instr_case_novalue(Op_div_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_div_p8_imm) {}
			instr_case_novalue(Op_neg_p8) {}

			instr_case_novalue(Op_cmpEq_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_p64_imm) {}
			instr_case_novalue(Op_cmpNeq_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_p64_imm) {}
			instr_case_novalue(Op_cmpEq_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_p32_imm) {}
			instr_case_novalue(Op_cmpNeq_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_p32_imm) {}
			instr_case_novalue(Op_cmpEq_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_p16_imm) {}
			instr_case_novalue(Op_cmpNeq_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_p16_imm) {}
			instr_case_novalue(Op_cmpEq_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpEq_p8_imm) {}
			instr_case_novalue(Op_cmpNeq_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpNeq_p8_imm) {}

			instr_case_novalue(Op_cmpGt_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_p64_imm) {}
			instr_case_novalue(Op_cmpGe_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_p64_imm) {}
			instr_case_novalue(Op_cmpGt_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_p32_imm) {}
			instr_case_novalue(Op_cmpGe_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_p32_imm) {}
			instr_case_novalue(Op_cmpGt_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_p16_imm) {}
			instr_case_novalue(Op_cmpGe_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_p16_imm) {}
			instr_case_novalue(Op_cmpGt_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGt_p8_imm) {}
			instr_case_novalue(Op_cmpGe_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpGe_p8_imm) {}

			instr_case_novalue(Op_ucmpGt_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_p64_imm) {}
			instr_case_novalue(Op_ucmpGe_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_p64_imm) {}
			instr_case_novalue(Op_ucmpGt_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_p32_imm) {}
			instr_case_novalue(Op_ucmpGe_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_p32_imm) {}
			instr_case_novalue(Op_ucmpGt_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_p16_imm) {}
			instr_case_novalue(Op_ucmpGe_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_p16_imm) {}
			instr_case_novalue(Op_ucmpGt_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGt_p8_imm) {}
			instr_case_novalue(Op_ucmpGe_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpGe_p8_imm) {}

			instr_case_novalue(Op_cmpLt_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_p64_imm) {}
			instr_case_novalue(Op_cmpLe_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_p64_imm) {}
			instr_case_novalue(Op_cmpLt_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_p32_imm) {}
			instr_case_novalue(Op_cmpLe_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_p32_imm) {}
			instr_case_novalue(Op_cmpLt_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_p16_imm) {}
			instr_case_novalue(Op_cmpLe_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_p16_imm) {}
			instr_case_novalue(Op_cmpLt_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLt_p8_imm) {}
			instr_case_novalue(Op_cmpLe_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_cmpLe_p8_imm) {}

			instr_case_novalue(Op_ucmpLt_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_p64_imm) {}
			instr_case_novalue(Op_ucmpLe_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_p64_imm) {}
			instr_case_novalue(Op_ucmpLt_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_p32_imm) {}
			instr_case_novalue(Op_ucmpLe_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_p32_imm) {}
			instr_case_novalue(Op_ucmpLt_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_p16_imm) {}
			instr_case_novalue(Op_ucmpLe_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_p16_imm) {}
			instr_case_novalue(Op_ucmpLt_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLt_p8_imm) {}
			instr_case_novalue(Op_ucmpLe_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_ucmpLe_p8_imm) {}

			instr_case_novalue(Op_fcmpEq_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpEq_p64_imm) {}
			instr_case_novalue(Op_fcmpNeq_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpNeq_p64_imm) {}
			instr_case_novalue(Op_fcmpGt_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGt_p64_imm) {}
			instr_case_novalue(Op_fcmpGe_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGe_p64_imm) {}
			instr_case_novalue(Op_fcmpLt_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLt_p64_imm) {}
			instr_case_novalue(Op_fcmpLe_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLe_p64_imm) {}

			instr_case_novalue(Op_fcmpEq_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpEq_p32_imm) {}
			instr_case_novalue(Op_fcmpNeq_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpNeq_p32_imm) {}
			instr_case_novalue(Op_fcmpGt_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGt_p32_imm) {}
			instr_case_novalue(Op_fcmpGe_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpGe_p32_imm) {}
			instr_case_novalue(Op_fcmpLt_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLt_p32_imm) {}
			instr_case_novalue(Op_fcmpLe_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fcmpLe_p32_imm) {}

			instr_case_novalue(Op_cmpNull_pptr) {}

			instr_case_novalue(Op_fadd_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fadd_p64_imm) {}
			instr_case_novalue(Op_fadd_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fadd_p32_imm) {}

			instr_case_novalue(Op_fsub_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fsub_p64_imm) {}
			instr_case_novalue(Op_fsub_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fsub_p32_imm) {}

			instr_case_novalue(Op_fmul_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fmul_p64_imm) {}
			instr_case_novalue(Op_fmul_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fmul_p32_imm) {}

			instr_case_novalue(Op_fdiv_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fdiv_p64_imm) {}
			instr_case_novalue(Op_fdiv_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_fdiv_p32_imm) {}

			instr_case_novalue(Op_fneg_p64) {}
			instr_case_novalue(Op_fneg_p32) {}

			instr_case_novalue(Op_umul_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_p64_imm) {}
			instr_case_novalue(Op_umod_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_p64_imm) {}
			instr_case_novalue(Op_udiv_p64_p64) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_p64_imm) {}

			instr_case_novalue(Op_umul_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_p32_imm) {}
			instr_case_novalue(Op_umod_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_p32_imm) {}
			instr_case_novalue(Op_udiv_p32_p32) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_p32_imm) {}

			instr_case_novalue(Op_umul_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_p16_imm) {}
			instr_case_novalue(Op_umod_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_p16_imm) {}
			instr_case_novalue(Op_udiv_p16_p16) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_p16_imm) {}

			instr_case_novalue(Op_umul_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umul_p8_imm) {}
			instr_case_novalue(Op_umod_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_umod_p8_imm) {}
			instr_case_novalue(Op_udiv_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_udiv_p8_imm) {}

			instr_case_novalue(Op_log_and_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_log_and_p8_imm) {}
			instr_case_novalue(Op_log_or_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_log_or_p8_imm) {}
			instr_case_novalue(Op_log_xor_p8_p8) {
				validatePlacePrimitiveArgumentsSameType(instruction, current_stack);
			}
			instr_case_novalue(Op_log_xor_p8_imm) {}
			instr_case_novalue(Op_log_not_p8) {}


			instr_case(Op_variantSetInner_pvnt_type, instr) {
				const auto variant_type = getPlaceType(instr.variant, current_stack)
				                              ->getKindAs<valid_type::finalized::Variant>();
				valid_type::ValidTypeID wanted_type
					= types_ctx.at(instr.inner_type.type_name)->getID();

				if (!variant_type->alternatives_set.contains(wanted_type))
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantGetInner_pptr_pvnt_type, instr) {
				const auto variant_type = getPlaceType(instr.variant, current_stack)
				                              ->getKindAs<valid_type::finalized::Variant>();
				const auto pointer_type = getPlaceType(instr.dst_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				auto wanted_type = types_ctx.at(pointer_type->inner);
				if (!variant_type->alternatives_set.contains(wanted_type->getID()))
					throw VariantTypeMismatchError(instr);

				if (instr.expected_type != wanted_type->getName())
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantSetInner_pptr_type, instr) {
				const auto variant_pointer = getPlaceType(instr.variant_ptr, current_stack)
				                                 ->getKindAs<valid_type::finalized::Pointer>();

				const auto variant_type = expectPointerType<valid_type::finalized::Variant>(
					variant_pointer, types_ctx, instr
				);

				auto wanted_type = types_ctx.at(instr.inner_type.type_name);
				if (!variant_type->alternatives_set.contains(wanted_type->getID()))
					throw VariantTypeMismatchError(instr);
			}
			instr_case(Op_variantGetInner_pptr_pptr_type, instr) {
				const auto pointer_type = getPlaceType(instr.dst_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				auto wanted_type = types_ctx.at(pointer_type->inner);


				const auto variant_pointer = getPlaceType(instr.variant_ptr, current_stack)
				                                 ->getKindAs<valid_type::finalized::Pointer>();
				const auto variant_type = expectPointerType<valid_type::finalized::Variant>(
					variant_pointer, types_ctx, instr
				);

				if (!variant_type->alternatives_set.contains(wanted_type->getID()))
					throw VariantTypeMismatchError(instr);

				if (instr.expected_type != wanted_type->getName())
					throw VariantTypeMismatchError(instr);
			}
			instr_case_novalue(Op_label, Op_jmp_label, Op_jmpIf_label, Op_jmpIfNot_label) {}
			instr_case_novalue(Op_call_func, Op_call_builtinfunc, Op_call_cfunc, Op_call_ffifunc) {}
			instr_case_novalue(Op_set_threadctx) {}
			instr_case(Op_virtual_call_pptr_method, instr) {
				// For a method call to be valid it has to be present in the interface.
				const auto pointer_type = getPlaceType(instr.object_ptr, current_stack)
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
			instr_case_novalue(Op_ret_tailcall_func, Op_ret, Op_deinit) {}
			instr_case_novalue(Op_input_p64, Op_output_p64, Op_input_p32, Op_output_p32) {}
			instr_case(Op_setVTable_pptr_type, instr) {
				const auto pointer_type = getPlaceType(instr.object_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				if (pointer_type->inner != types_ctx.at(instr.type.type_name)->getID())
					throw VTableTypeMismatchError(instr);
			}
			instr_case(Op_resetVTable_pptr, instr) {
				const auto pointer_type = getPlaceType(instr.object_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				const auto structure = expectPointerType<valid_type::finalized::Structure>(
					pointer_type, types_ctx, instr
				);
				if (!structure->inheritance_metadata) throw NotAClassTypeError(instr);
			}
			instr_case(Op_downcast_pptr_pptr, instr) {
				validateClassCast<InvalidDowncastError>(instr, current_stack);
			}
			instr_case_novalue(Op_free_pptr) {}
			instr_case(Op_store_pptr_pany, instr) {
				const auto pointer_type = getPlaceType(instr.dst_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				CRef<valid_type::ValidType> other_type = getPlaceType(instr.src, current_stack);
				if (pointer_type->inner != other_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_load_pany_pptr, instr) {
				const auto pointer_type = getPlaceType(instr.src_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				CRef<valid_type::ValidType> other_type = getPlaceType(instr.dst, current_stack);
				if (pointer_type->inner != other_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_ref_pptr_pany, instr) {
				const auto pointer_type = getPlaceType(instr.dst_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				CRef<valid_type::ValidType> other_type = getPlaceType(instr.src, current_stack);
				if (pointer_type->inner != other_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_ref_pptr_pvnt, instr) {
				const auto pointer_type = getPlaceType(instr.dst_ptr, current_stack)
				                              ->getKindAs<valid_type::finalized::Pointer>();
				const auto&                 global_entry = globals.at(instr.src.var_name);
				CRef<valid_type::ValidType> global_type  = types_ctx.at(global_entry->type);
				if (pointer_type->inner != global_type->getID())
					throw PointerTypeMismatchError(instr);
			}
			instr_case(Op_structLea_pptr_pptr_field, instr) {
				const auto destination = getPlaceType(instr.dst_ptr, current_stack)
				                             ->getKindAs<valid_type::finalized::Pointer>();

				const auto ztruct_pointer = getPlaceType(instr.src_data_ptr, current_stack)
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
			instr_case(Op_structLoad_pany_pptr_field, instr) {
				const auto& destination = getPlaceType(instr.dst, current_stack);

				const auto ztruct_pointer = getPlaceType(instr.src_data_ptr, current_stack)
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
			instr_case(Op_structStore_pptr_pany_field, instr) {
				const auto& source = getPlaceType(instr.src, current_stack);

				const auto ztruct_pointer = getPlaceType(instr.dst_data_ptr, current_stack)
				                                ->getKindAs<valid_type::finalized::Pointer>();
				const auto ztruct = expectPointerType<valid_type::finalized::Structure>(
					ztruct_pointer, types_ctx, instr
				);

				validateStructFieldType(
					*types_ctx.at(ztruct_pointer->inner), *ztruct, instr.field, source->getID(), instr
				);
			}

			instr_case(Op_structLea_pptr_pste_field, instr) {
				auto dst = getPlaceType(instr.dst_ptr, current_stack)
				               ->getKindAs<valid_type::finalized::Pointer>();
				auto src        = getPlaceType(instr.src_data_struct, current_stack);
				auto src_struct = src->getKindAs<valid_type::finalized::Structure>();
				validateStructFieldType(*src, *src_struct, instr.field, dst->inner, instr);
			}
			instr_case(Op_structLoad_pany_pste_field, instr) {
				auto target_type = types_ctx.at(getPlaceType(instr.dst, current_stack)->getID());
				auto source_type
					= types_ctx.at(getPlaceType(instr.src_data_struct, current_stack)->getID());
				validateStructFieldType(
					*source_type,
					*source_type->getKindAs<valid_type::finalized::Structure>(),
					instr.field,
					target_type->getID(),
					instr
				);
			}
			instr_case(Op_structStore_pste_pany_field, instr) {
				auto target_type
					= types_ctx.at(getPlaceType(instr.dst_data_struct, current_stack)->getID());
				auto source_type = types_ctx.at(getPlaceType(instr.src, current_stack)->getID());
				validateStructFieldType(
					*target_type,
					*target_type->getKindAs<valid_type::finalized::Structure>(),
					instr.field,
					source_type->getID(),
					instr
				);
			}
			instr_case(Op_fixedSizeTableLea_pptr_pptr_p64, instr) {
				const auto destination = getPlaceType(instr.dst_ptr, current_stack)
				                             ->getKindAs<valid_type::finalized::Pointer>();

				const auto table_pointer = getPlaceType(instr.src_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::FixedSizeTable>(
					table_pointer, types_ctx, instr
				);

				if (destination->inner != table_type->inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableLoad_pany_pptr_p64, instr) {
				const auto& destination = getPlaceType(instr.dst, current_stack);

				const auto table_pointer = getPlaceType(instr.src_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::FixedSizeTable>(
					table_pointer, types_ctx, instr
				);

				if (destination->getID() != table_type->inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableStore_pptr_pany_p64, instr) {
				const auto& source = getPlaceType(instr.src, current_stack);

				const auto table_pointer = getPlaceType(instr.dst_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::FixedSizeTable>(
					table_pointer, types_ctx, instr
				);

				if (table_type->inner != source->getID())
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableLea_pptr_pfst_p64, instr) {
				const auto destination = getPlaceType(instr.dst_ptr, current_stack)
				                             ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = getPlaceType(instr.src_table, current_stack)
				                            ->getKindAs<valid_type::finalized::FixedSizeTable>();

				if (destination->inner != table_type->inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableLoad_pany_pfst_p64, instr) {
				const auto& destination = getPlaceType(instr.dst, current_stack);
				const auto  table_type  = getPlaceType(instr.src_table, current_stack)
				                            ->getKindAs<valid_type::finalized::FixedSizeTable>();

				if (destination->getID() != table_type->inner)
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_fixedSizeTableStore_pfst_pany_p64, instr) {
				const auto& source     = getPlaceType(instr.src, current_stack);
				const auto  table_type = getPlaceType(instr.dst_table, current_stack)
				                            ->getKindAs<valid_type::finalized::FixedSizeTable>();

				if (table_type->inner != source->getID())
					throw FixedSizeTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableLea_pptr_pptr_p64, instr) {
				const auto destination = getPlaceType(instr.dst_ptr, current_stack)
				                             ->getKindAs<valid_type::finalized::Pointer>();

				const auto table_pointer = getPlaceType(instr.src_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);

				if (destination->inner != table_type->inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableLoad_pany_pptr_p64, instr) {
				const auto& destination   = getPlaceType(instr.dst, current_stack);
				const auto  table_pointer = getPlaceType(instr.src_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);
				if (destination->getID() != table_type->inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableStore_pptr_pany_p64, instr) {
				const auto& source        = getPlaceType(instr.src, current_stack);
				const auto  table_pointer = getPlaceType(instr.dst_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);

				if (table_type->inner != source->getID())
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_dynTableReAlloc_pptr_type_p64, instr) {
				const auto table_pointer = getPlaceType(instr.dst_table_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);

				valid_type::ValidTypeID allocated_type
					= types_ctx.at(instr.table_type.type_name)->getID();
				if (allocated_type != table_pointer->inner)
					throw InvalidArgumentTypeError(instr.table_type);
			}
			instr_case(Op_strOutput_pptr, instr) {
				const auto table_pointer = getPlaceType(instr.string_ptr, current_stack)
				                               ->getKindAs<valid_type::finalized::Pointer>();
				const auto table_type = expectPointerType<valid_type::finalized::DynamicTable>(
					table_pointer, types_ctx, instr
				);
				if (types_ctx.at(table_type->inner)->getName() != "byte")
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case(Op_fstToDynTable_pptr_pptr, instr) {
				const auto src_ptr = getPlaceType(instr.src_table_ptr, current_stack)
				                         ->getKindAs<valid_type::finalized::Pointer>();
				const auto src_table = expectPointerType<valid_type::finalized::FixedSizeTable>(
					src_ptr, types_ctx, instr
				);
				const auto dst_ptr = getPlaceType(instr.dst_table_ptr, current_stack)
				                         ->getKindAs<valid_type::finalized::Pointer>();
				const auto dst_table = expectPointerType<valid_type::finalized::DynamicTable>(
					dst_ptr, types_ctx, instr
				);
				if (src_table->inner != dst_table->inner)
					throw DynamicTableTypeMismatchError(instr);
			}
			instr_case_novalue(Op_nop, Op_exit, Op_initFromVmValue) {}
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
	requires(std::is_same_v<Instr, std::remove_cvref_t<Op_upcast_pptr_pptr>> || std::is_same_v<Instr, std::remove_cvref_t<Op_downcast_pptr_pptr>>)
	void validateClassCast(const Instr& instruction, const LocalStack& current_stack) const {
		auto higher_ptr_tod = getPlaceType(instruction.dst, current_stack);
		auto lower_ptr_tod  = getPlaceType(instruction.src, current_stack);
		if constexpr (std::is_same_v<Instr, std::remove_cvref_t<Op_downcast_pptr_pptr>>)
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
		const auto curr_type      = VISIT(local, l, return getPlaceType(l, current_stack));
		const auto new_type       = types_ctx.at(type.type_name);
		const auto curr_primitive = curr_type->getKindAs<valid_type::finalized::Primitive>();

		const auto new_primitive = new_type->maybeGetKindAs<valid_type::finalized::Primitive>()
		                               .expect<NonPrimitiveCastError>(type);
		if (curr_primitive->size != new_primitive->size) throw CastSizeMismatchError(instruction);
	}

	void validateFunctionEnd() const {
		if (function.body.empty()
		    || (stack_before_instr.back().has_value()
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

	LocalStackDb traverseControlFlowGraph() {
		LocalStackDbBuilder builder(types_ctx);

		auto start_state = LocalStackDbBuilder::EMPTY;
		using namespace std::views;
		for (auto [idx, ret]: enumerate(function.signature.result_types))
			start_state = builder.push(
				start_state, base::StrID(base::strConcat("ret", idx).c_str()), ret.str
			);

		for (auto [idx, param]: enumerate(function.signature.parameters))
			start_state = builder.push(
				start_state, base::StrID(base::strConcat("arg", idx).c_str()), param.str
			);

		LocalStack local_stack(
			types_ctx,
			Ref<LocalStackDbBuilder>{ &builder },
			start_state,
			function.signature.result_types.size()
		);

		stack_before_instr.resize(function.body.size(), std::nullopt);
		std::vector<std::tuple<usize, LocalStack>> dfs_stack{
			{ function.body.size(), local_stack }  // sentinel
		};
		usize index = 0;

		auto& instructions = function.body;

		while (index != function.body.size()) {
			validateArgTypes(instructions[index], local_stack);

			validateArgTypesNonTrivially(instructions[index], local_stack);

			instr_match(instructions[index]) {
				instr_case(Op_init_pany_type, instr) {
					// this is the only exception from the rule "save stack state before instruction"
					// it is needed to properly lower the name of the variable for the compilation
					local_stack.push(instr.var, instr.type);
					stack_before_instr[index] = local_stack.getStateID();
					index++;
				}
				instr_case(Op_deinit, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					local_stack.pop(instr);
					index++;
				}
				instr_case(Op_label, instr) {
					match_optional(stack_before_instr[index]) {
						opt_some(label_state) {
							if (!local_stack.eqStack(label_state, local_stack.getStateID()))
								throw StackStructureMismatchError(
									instr, jumps_to_label.at(instr.label.label_name)
								);
							std::tie(index, local_stack) = dfs_stack.back();
							dfs_stack.pop_back();
						}
						opt_none {
							stack_before_instr[index] = local_stack.getStateID();
							index++;
						}
					}
				}
				instr_case(Op_jmp_label, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					index                     = getLabelTarget(instr.label);
				}
				instr_case(Op_jmpIf_label, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					index++;
					dfs_stack.emplace_back(getLabelTarget(instr.label), local_stack);
				}
				instr_case(Op_jmpIfNot_label, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					index++;
					dfs_stack.emplace_back(getLabelTarget(instr.label), local_stack);
				}
				instr_case(Op_ret, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateRet(local_stack, instr, function.signature);
					std::tie(index, local_stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_case(Op_call_func, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_call_builtinfunc, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_call_cfunc, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_call_ffifunc, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_virtual_call_pptr_method, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateMethodCallAndPop(local_stack, instr);
					index++;
				}
				instr_case(Op_ret_tailcall_func, instr) {
					stack_before_instr[index] = local_stack.getStateID();
					validateTailcall(local_stack, instr, function.signature);
					std::tie(index, local_stack) = dfs_stack.back();
					dfs_stack.pop_back();
				}
#define HANDLE_CAST(SIZE)                                          \
	instr_case(Op_cast_p##SIZE##_type, instr) {                    \
		stack_before_instr[index] = local_stack.getStateID();      \
		local_stack.castPrimitive(instr.value, instr.target_type); \
		index++;                                                   \
	}

				FOR_EACH(HANDLE_CAST, 8, 16, 32, 64)
#undef HANDLE_CAST
				instr_default {
					stack_before_instr[index] = local_stack.getStateID();
					index++;
				}
			}
		}

		return builder.finalize();
	}

	void validateSignature() {
		if (function.name.str == base::StrID("main")) {
			if (function.signature.result_types.size() != 1)
				throw InvalidMainReturnType(function.signature, false);
			if (function.signature.result_types[0].str != base::StrID("i64"))
				throw InvalidMainReturnType(function.signature, true);
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
		const ObjIdNameMap<FFIFunction>&                 ffi_signatures,
		const Function&                                  function
	):
		  types_ctx(types_ctx),
		  globals(globals),
		  signatures(signatures),
		  ext_c_signatures(ext_c_signatures),
		  ffi_signatures(ffi_signatures),
		  function(function) {}

	std::tuple<std::vector<Instruction>, std::vector<StackStateID>, LocalStackDb> validateAndExtractReachableCode(
	) {
		validateSignature();
		preprocessLabels();
		LocalStackDb db = traverseControlFlowGraph();
		validateFunctionEnd();

		std::vector<Instruction>  body;
		std::vector<StackStateID> stack_states;
		for (usize idx = 0; idx < function.body.size(); idx++) {
			if_opt_some(stack_before_instr[idx], state) {
				body.emplace_back(function.body[idx]);
				stack_states.emplace_back(state);
			}
		}

		return { body, stack_states, db };
	}
};

vm::code::valid_function::ValidFunction vm::code::detail::validateAndExtractReachableCode(
	const valid_type::ValidTypeMap&                  types,
	const ObjIdNameMap<GlobalData>&                  globals_map,
	const base::HashMap<base::StrID, FuncSignature>& signatures,
	const ObjIdNameMap<ExternalCFunction>&           ext_c_signatures,
	const FlagContext&                               flag_context,
	const ObjIdNameMap<FFIFunction>&                 ffi_signatures,
	const Function&                                  function
) {
	FuncSignature signature = signatures.at(function.name);


	FunctionValidator validator(
		types, globals_map, signatures, ext_c_signatures, ffi_signatures, function
	);

	valid_function::ValidFunction new_function;
	new_function.name = function.name;
	std::tie(new_function.body, new_function.stack_states, new_function.local_stack)
		= validator.validateAndExtractReachableCode();
	new_function.bytecode_pos = function.bytecode_pos;
	new_function.signature    = signature;
	new_function.flags        = flag_context.getFlagsForFunction(function.name.str);

	return new_function;
}
