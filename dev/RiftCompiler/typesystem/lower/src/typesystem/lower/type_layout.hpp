#pragma once

#include "size_constants.hpp"

#include <base/ref.hpp>
#include <typesystem/higher/type_info.hpp>
#include <typesystem/higher/types.hpp>
#include <typesystem/higher/type_desc.hpp>

#include <query_framework/query_int.hpp>

#include <variant>

/**
 * @brief The namespace of all definitions of the Lower Type System.
 * Short for "Type System: Low(er)".
 */
namespace tsl {
	/**
	 * @brief The abstract base class of a Type Layout object.
	 */
	class TypeLayoutABC {
		/**
		 * @brief The runtime contiguous size of a type's memory layout.
		 */
		usize size;

		/**
		 * @brief The source type of a memory layout.
		 */
		tsh::TypeInfo source_type;

	public:
		/**
		 * @brief Get the total size of a layout.
		 * @return The total size of a layout.
		 */
		usize getSize() { return size; }

		/**
		 * @brief Get the source type of a layout.
		 * @return The source type of a layout.
		 */
		tsh::TypeInfo getSourceType() { return source_type; }

	protected:
		TypeLayoutABC(usize size, tsh::TypeInfo source_type):
			  size(size),
			  source_type(source_type) {}
	};

	class TypeLayout;

	/**
	 * @brief Layout of a type that does not require any representation in memory.
	 *
	 * @note This is only viable for Unit, as it only has a single value,
	 * which carries no information. While Void also carries no information, it has no values
	 * whatsoever, so considering a layout for it is invalid and should not be "useful".
	 */
	class EmptyLayout: public TypeLayoutABC {
	public:
		EmptyLayout(tsh::UnitInfo unit_info): TypeLayoutABC(0, unit_info) {}
	};

	/**
	 * @brief Layout of a type that has integral-like low level behaviour.
	 *
	 * Valid candidates include, of course, integers, but also bytes, bools, and characters.
	 */
	class IntegralTypeLayout: public TypeLayoutABC {
	public:
		IntegralTypeLayout(tsh::ByteInfo byte_info): TypeLayoutABC(tsl::BYTE_SIZE, byte_info) {}

		IntegralTypeLayout(tsh::BoolInfo bool_info): TypeLayoutABC(tsl::BOOL_SIZE, bool_info) {}

		IntegralTypeLayout(tsh::CharInfo char_info): TypeLayoutABC(tsl::CHAR_SIZE, char_info) {}

		IntegralTypeLayout(tsh::IntegralInfo integral_info):
			  TypeLayoutABC(integral_info.getSize(), integral_info) {}
	};

	/**
	 * @brief Layout of a type that has float-like low level behaviour.
	 */
	class FloatTypeLayout: public TypeLayoutABC {
	public:
		FloatTypeLayout(tsh::FloatInfo float_info):
			  TypeLayoutABC(float_info.getSize(), float_info) {}
	};

	/**
	 * @brief Layout of a variant type.
	 */
	class VariantTypeLayout: public TypeLayoutABC {
	public:
		VariantTypeLayout(tsh::VariantInfo variant_info);
	};

	/**
	 * @brief Layout of a type that has struct-like low level behaviour.
	 *
	 * Valid source types are tuples and classes. A Variant is not a valid
	 * source type because it is different and special enough to be considered separately.
	 */
	class StructuralTypeLayout: public TypeLayoutABC {
	public:
		StructuralTypeLayout(tsh::TupleInfo tuple_info);
	};

	/**
	 * @brief Layout of a functional object or pointer type.
	 */
	class FunctionalLayout: public TypeLayoutABC {
	public:
		FunctionalLayout(tsh::FunctionInfo function_info);
	};

	/**
	 * @brief Layout of a type that has pointer-like low level behaviour.
	 */
	class PointerLayout: public TypeLayoutABC {
		MRef<TypeLayout> pointee;

	public:
		Ref<TypeLayout> operator->() { return pointee.toOpt().value(); }

		/**
		 * @brief Construct a PointerLayout for a RawPointer.
		 * @param raw_pointer_info The source RawPointer.
		 */
		PointerLayout(tsh::RawPointerInfo raw_pointer_info):
			  TypeLayoutABC(POINTER_SIZE, raw_pointer_info),
			  pointee() {}

		/**
		 * @brief Construct a PointerLayout from a typed Pointer.
		 * @param pointer_info The source typed Pointer.
		 * @param ctx The query::Context for constructing the TypeLayout of the pointee.
		 */
		PointerLayout(tsh::PointerInfo pointer_info, query::Context& ctx);

		// There is NO constructor from ReferenceInfo because
		// ReferenceInfo will be soon removed.

		/**
		 * @brief Construct a PointerLayout directly from the PointerLayout of the pointee.
		 * @param pointee The PointerLayout of the pointee.
		 */
		PointerLayout(TypeLayout pointee);
	};

	/**
	 * @brief The ADT representing the layout of a type.
	 */
	class TypeLayout:
		  public std::variant<
			  EmptyLayout,
			  IntegralTypeLayout,
			  FloatTypeLayout,
			  VariantTypeLayout,
			  StructuralTypeLayout,
			  FunctionalLayout,
			  PointerLayout> {
	public:
		using Base = std::variant<
			EmptyLayout,
			IntegralTypeLayout,
			FloatTypeLayout,
			VariantTypeLayout,
			StructuralTypeLayout,
			FunctionalLayout,
			PointerLayout>;

		Base& operator()() { return *this; }

		using Base::Base;
	};
}
