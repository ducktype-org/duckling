#pragma once

#include "errors.hpp"

#include <base/ref.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace vm::code::func_validator_helpers {
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
		using RequiredAfter = std::tuple<
			Op_fixedSizeTableLea_lptr_lptr,
			Op_fixedSizeTableLoad_lany_lptr,
			Op_fixedSizeTableStore_lptr_lany>;
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
